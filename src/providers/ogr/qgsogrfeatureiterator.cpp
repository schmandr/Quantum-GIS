#include "qgsogrfeatureiterator.h"

#include "qgsogrprovider.h"

#include "qgsapplication.h"
#include "qgslogger.h"
#include "qgsgeometry.h"

#include <QTextCodec>

// using from provider:
// - setRelevantFields(), mRelevantFieldsForNextFeature
// - ogrLayer
// - mFetchFeaturesWithoutGeom
// - mAttributeFields
// - mEncoding


QgsOgrFeatureIterator::QgsOgrFeatureIterator( QgsOgrProvider* p, const QgsFeatureRequest& request )
    : QgsAbstractFeatureIterator( request ), P( p )
{
  // set the selection rectangle pointer to 0
  mSelectionRectangle = 0;

  mFeatureFetched = false;

  ensureRelevantFields();

  // spatial query to select features
  if ( mRequest.filterType() == QgsFeatureRequest::FilterRect )
  {
    OGRGeometryH filter = 0;
    QString wktExtent = QString( "POLYGON((%1))" ).arg( mRequest.filterRect().asPolygon() );
    QByteArray ba = wktExtent.toAscii();
    const char *wktText = ba;

    if ( mRequest.flags() & QgsFeatureRequest::ExactIntersect )
    {
      // store the selection rectangle for use in filtering features during
      // an identify and display attributes
      if ( mSelectionRectangle )
        OGR_G_DestroyGeometry( mSelectionRectangle );

      OGR_G_CreateFromWkt(( char ** )&wktText, NULL, &mSelectionRectangle );
      wktText = ba;
    }

    OGR_G_CreateFromWkt(( char ** )&wktText, NULL, &filter );
    QgsDebugMsg( "Setting spatial filter using " + wktExtent );
    OGR_L_SetSpatialFilter( P->ogrLayer, filter );
    OGR_G_DestroyGeometry( filter );
  }
  else
  {
    OGR_L_SetSpatialFilter( P->ogrLayer, 0 );
  }

  //start with first feature
  rewind();
}

QgsOgrFeatureIterator::~QgsOgrFeatureIterator()
{
  close();
}

void QgsOgrFeatureIterator::ensureRelevantFields()
{
  bool needGeom = ( mRequest.filterType() == QgsFeatureRequest::FilterRect ) || !( mRequest.flags() & QgsFeatureRequest::NoGeometry );
  QgsAttributeList attrs = ( mRequest.flags() & QgsFeatureRequest::SubsetOfAttributes ) ? mRequest.subsetOfAttributes() : P->attributeIndexes();
  P->setRelevantFields( needGeom, attrs );
  P->mRelevantFieldsForNextFeature = true;
}


bool QgsOgrFeatureIterator::nextFeature( QgsFeature& feature )
{
  feature.setValid( false );

  if ( mClosed )
    return false;

  if ( !P->mRelevantFieldsForNextFeature )
    ensureRelevantFields();

  if ( mRequest.filterType() == QgsFeatureRequest::FilterFid )
  {
    // make sure that only one next feature call returns a valid feature!
    if ( mFeatureFetched )
      return false;

    OGRFeatureH fet = OGR_L_GetFeature( P->ogrLayer, FID_TO_NUMBER( mRequest.filterFid() ) );
    if ( !fet )
      return false;

    // skip features without geometry
    if ( !OGR_F_GetGeometryRef( fet ) && !P->mFetchFeaturesWithoutGeom )
    {
      OGR_F_Destroy( fet );
      return false;
    }

    readFeature( fet, feature );
    feature.setValid( true );
    mFeatureFetched = true;
    return true;
  }

  OGRFeatureH fet;
  QgsRectangle selectionRect;

  while (( fet = OGR_L_GetNextFeature( P->ogrLayer ) ) )
  {
    // skip features without geometry
    if ( !P->mFetchFeaturesWithoutGeom && !OGR_F_GetGeometryRef( fet ) )
    {
      OGR_F_Destroy( fet );
      continue;
    }

    readFeature( fet, feature );

    if ( mRequest.flags() & QgsFeatureRequest::ExactIntersect )
    {
      //precise test for intersection with search rectangle
      //first make QgsRectangle from OGRPolygon
      OGREnvelope env;
      memset( &env, 0, sizeof( env ) );
      if ( mSelectionRectangle )
        OGR_G_GetEnvelope( mSelectionRectangle, &env );
      if ( env.MinX != 0 || env.MinY != 0 || env.MaxX != 0 || env.MaxY != 0 ) //if envelope is invalid, skip the precise intersection test
      {
        selectionRect.set( env.MinX, env.MinY, env.MaxX, env.MaxY );
        if ( !feature.geometry() || !feature.geometry()->intersects( selectionRect ) )
        {
          OGR_F_Destroy( fet );
          continue;
        }
      }
    }

    // we have a feature, end this cycle
    feature.setValid( true );
    OGR_F_Destroy( fet );
    return true;

  } // while

  QgsDebugMsg( "Feature is null" );

  // probably should reset reading here
  rewind();

  return false;
}


bool QgsOgrFeatureIterator::rewind()
{
  if ( mClosed )
    return false;

  OGR_L_ResetReading( P->ogrLayer );

  return true;
}


bool QgsOgrFeatureIterator::close()
{
  if ( mClosed )
    return false;

  if ( mSelectionRectangle )
  {
    OGR_G_DestroyGeometry( mSelectionRectangle );
    mSelectionRectangle = 0;
  }

  mClosed = true;
  return true;
}


void QgsOgrFeatureIterator::getFeatureAttribute( OGRFeatureH ogrFet, QgsFeature & f, int attindex )
{
  OGRFieldDefnH fldDef = OGR_F_GetFieldDefnRef( ogrFet, attindex );

  if ( ! fldDef )
  {
    QgsDebugMsg( "ogrFet->GetFieldDefnRef(attindex) returns NULL" );
    return;
  }

  QVariant value;

  if ( OGR_F_IsFieldSet( ogrFet, attindex ) )
  {
    switch ( P->mAttributeFields[attindex].type() )
    {
      case QVariant::String: value = QVariant( P->mEncoding->toUnicode( OGR_F_GetFieldAsString( ogrFet, attindex ) ) ); break;
      case QVariant::Int: value = QVariant( OGR_F_GetFieldAsInteger( ogrFet, attindex ) ); break;
      case QVariant::Double: value = QVariant( OGR_F_GetFieldAsDouble( ogrFet, attindex ) ); break;
        //case QVariant::DateTime: value = QVariant(QDateTime::fromString(str)); break;
      default: assert( NULL && "unsupported field type" );
    }
  }
  else
  {
    value = QVariant( QString::null );
  }

  f.setAttribute( attindex, value );
}



void QgsOgrFeatureIterator::readFeature( OGRFeatureH fet, QgsFeature& feature )
{
  feature.setFeatureId( OGR_F_GetFID( fet ) );
  feature.initAttributes( P->fields().count() );
  feature.setFields( &P->mAttributeFields ); // allow name-based attribute lookups

  // fetch geometry
  if ( !( mRequest.flags() & QgsFeatureRequest::NoGeometry ) )
  {
    OGRGeometryH geom = OGR_F_GetGeometryRef( fet );

    if ( geom )
    {
      // get the wkb representation
      unsigned char *wkb = new unsigned char[OGR_G_WkbSize( geom )];
      OGR_G_ExportToWkb( geom, ( OGRwkbByteOrder ) QgsApplication::endian(), wkb );

      feature.setGeometryAndOwnership( wkb, OGR_G_WkbSize( geom ) );
    }
    else
    {
      feature.setGeometry( 0 );
    }
  }

  // fetch attributes
  if ( mRequest.flags() & QgsFeatureRequest::SubsetOfAttributes )
  {
    const QgsAttributeList& attrs = mRequest.subsetOfAttributes();
    for ( QgsAttributeList::const_iterator it = attrs.begin(); it != attrs.end(); ++it )
    {
      getFeatureAttribute( fet, feature, *it );
    }
  }
  else
  {
    // all attributes
    for ( int idx = 0; idx < P->mAttributeFields.count(); ++idx )
    {
      getFeatureAttribute( fet, feature, idx );
    }
  }
}