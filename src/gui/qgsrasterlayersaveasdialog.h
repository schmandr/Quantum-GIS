#ifndef QGSRASTERLAYERSAVEASDIALOG_H
#define QGSRASTERLAYERSAVEASDIALOG_H

#include "ui_qgsrasterlayersaveasdialogbase.h"
#include "qgsrectangle.h"
#include "qgscoordinatereferencesystem.h"

class QgsRasterDataProvider;
class QgsRasterFormatOptionsWidget;

class GUI_EXPORT QgsRasterLayerSaveAsDialog: public QDialog, private Ui::QgsRasterLayerSaveAsDialogBase
{
    Q_OBJECT
  public:
    enum Mode
    {
      RawDataMode,
      RenderedImageMode
    };
    enum CrsState
    {
      OriginalCrs,
      CurrentCrs,
      UserCrs
    };
    enum ExtentState
    {
      OriginalExtent,
      CurrentExtent,
      UserExtent,
    };
    enum ResolutionState
    {
      OriginalResolution,
      UserResolution
    };

    QgsRasterLayerSaveAsDialog( QgsRasterDataProvider* sourceProvider, const QgsRectangle& currentExtent, const QgsCoordinateReferenceSystem& currentCrs, QWidget* parent = 0, Qt::WindowFlags f = 0 );
    ~QgsRasterLayerSaveAsDialog();

    Mode mode() const;
    int nColumns() const;
    int nRows() const;
    double xResolution() const;
    double yResolution() const;
    int maximumTileSizeX() const;
    int maximumTileSizeY() const;
    bool tileMode() const;
    QString outputFileName() const;
    QString outputFormat() const;
    QgsCoordinateReferenceSystem outputCrs();
    QStringList createOptions() const;
    QgsRectangle outputRectangle() const;

    void hideFormat();
    void hideOutput();

  private slots:
    void on_mBrowseButton_clicked();
    void on_mSaveAsLineEdit_textChanged( const QString& text );
    void on_mCurrentExtentButton_clicked();
    void on_mOriginalExtentButton_clicked();
    void on_mFormatComboBox_currentIndexChanged( const QString& text );
    void on_mResolutionRadioButton_toggled( bool checked ) { toggleResolutionSize(); }
    void on_mOriginalResolutionPushButton_clicked() { setOriginalResolution(); }
    void on_mXResolutionLineEdit_textEdited( const QString & text ) { mResolutionState = UserResolution; recalcSize(); }
    void on_mYResolutionLineEdit_textEdited( const QString & text ) { mResolutionState = UserResolution; recalcSize(); }

    void on_mOriginalSizePushButton_clicked() { setOriginalSize(); }
    void on_mColumnsLineEdit_textEdited( const QString & text ) { mResolutionState = UserResolution; recalcResolution(); }
    void on_mRowsLineEdit_textEdited( const QString & text ) { mResolutionState = UserResolution; recalcResolution(); }

    void on_mXMinLineEdit_textEdited( const QString & text ) { mExtentState = UserExtent; extentChanged(); }
    void on_mXMaxLineEdit_textEdited( const QString & text ) { mExtentState = UserExtent; extentChanged(); }
    void on_mYMinLineEdit_textEdited( const QString & text ) { mExtentState = UserExtent; extentChanged(); }
    void on_mYMaxLineEdit_textEdited( const QString & text ) { mExtentState = UserExtent; extentChanged(); }

    void on_mChangeCrsPushButton_clicked();

    void on_mCrsComboBox_currentIndexChanged( int index ) { crsChanged(); }

  private:
    QgsRasterDataProvider* mDataProvider;
    QgsRectangle mCurrentExtent;
    QgsCoordinateReferenceSystem mCurrentCrs;
    QgsCoordinateReferenceSystem mUserCrs;
    QgsCoordinateReferenceSystem mPreviousCrs;
    ExtentState mExtentState;
    ResolutionState mResolutionState;

    void setValidators();
    void setOutputExtent( const QgsRectangle& r, const QgsCoordinateReferenceSystem& srcCrs, ExtentState state );
    void extentChanged();
    void updateExtentStateMsg();
    void toggleResolutionSize();
    void setResolution( double xRes, double yRes, const QgsCoordinateReferenceSystem& srcCrs );
    void setOriginalResolution();
    void setOriginalSize();
    void recalcSize();
    void recalcResolution();
    void updateResolutionStateMsg();
    void recalcResolutionSize();
    void crsChanged();
    void updateCrsGroup();
};

#endif // QGSRASTERLAYERSAVEASDIALOG_H