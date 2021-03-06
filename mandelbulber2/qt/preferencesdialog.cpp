#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"
#include <QtCore>
#include <QFileDialog>
#include <QMessageBox>

cPreferencesDialog::cPreferencesDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::cPreferencesDialog)
{
	initFinished = false;
	ui->setupUi(this);

	gMainInterface->ConnectSignalsForSlidersInWindow(this);
	gMainInterface->SynchronizeInterfaceWindow(this, gPar, cInterface::write);
	ui->comboBox_ui_style_type->addItems(QStyleFactory::keys());
	ui->comboBox_ui_style_type->setCurrentIndex(gPar->Get<int>("ui_style_type"));
	ui->comboBox_ui_skin->setCurrentIndex(gPar->Get<int>("ui_skin"));
	ui->comboboxLanguage->addItems(systemData.supportedLanguages.values());

	if(systemData.supportedLanguages.contains(gPar->Get<QString>("language")))
	{
		QString value = systemData.supportedLanguages[gPar->Get<QString>("language")];
		int index = ui->comboboxLanguage->findText(value);
		ui->comboboxLanguage->setCurrentIndex(index);
	}
	else
	{
		int index = ui->comboboxLanguage->findText(systemData.locale.name());
		ui->comboboxLanguage->setCurrentIndex(index);
	}
	initFinished = true;
}

cPreferencesDialog::~cPreferencesDialog()
{
  delete ui;
}

void cPreferencesDialog::on_buttonBox_accepted()
{
	gMainInterface->SynchronizeInterfaceWindow(this, gPar, cInterface::read);

	QFont font = gMainInterface->mainWindow->font();
	font.setPixelSize(gPar->Get<int>("ui_font_size"));
	gMainInterface->mainWindow->setFont(font);

	QString value = ui->comboboxLanguage->currentText();
	QString key = systemData.supportedLanguages.key(value);
	gPar->Set<QString>("language", key);

	gPar->Set<int>("toolbar_icon_size", gPar->Get<int>("toolbar_icon_size"));
	gMainInterface->mainWindow->slotPopulateToolbar(true);
}

void cPreferencesDialog::on_pushButton_select_image_path_clicked()
{
  QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(this, tr("Select default directory for images"), QDir::toNativeSeparators(ui->text_default_image_path->text())));
  if(dir.length() > 0)
  {
  	ui->text_default_image_path->setText(dir);
  }
}

void cPreferencesDialog::on_pushButton_select_settings_path_clicked()
{
  QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(this, tr("Select default directory for settings files"), QDir::toNativeSeparators(ui->text_default_settings_path->text())));
  if(dir.length() > 0)
  {
  	ui->text_default_settings_path->setText(dir);
  }
}

void cPreferencesDialog::on_pushButton_select_textures_path_clicked()
{
  QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(this, tr("Select default directory for textures"), QDir::toNativeSeparators(ui->text_default_textures_path->text())));
  if(dir.length() > 0)
  {
  	ui->text_default_textures_path->setText(dir);
  }
}

void cPreferencesDialog::on_pushButton_clear_thumbnail_cache_clicked()
{
	QDir thumbnailDir(systemData.thumbnailDir);
	thumbnailDir.setFilter( QDir::AllEntries | QDir::NoDotAndDotDot );
	int thumbnailDirCount = thumbnailDir.count();

	//confirmation dialog before clearing
	QMessageBox::StandardButton reply;
	reply = QMessageBox::question(
		NULL,
		QObject::tr("Are you sure to clear the thumbnail cache?"),
		QObject::tr("There are currently %1 thumbnails cached. These will be deleted and rerendered when necessary.\n Clear now?").arg(thumbnailDirCount),
		QMessageBox::Yes|QMessageBox::No);

	if (reply == QMessageBox::Yes)
	{
		// match exact 32 char hash images, example filename: c0ad626d8c25ab6a25c8d19a53960c8a.png
		DeleteAllFilesFromDirectory(systemData.thumbnailDir, "????????????????????????????????.*");
	}
	else
	{
		return;
	}
}

void cPreferencesDialog::on_comboBox_ui_style_type_currentIndexChanged(int index)
{
	if(!initFinished) return;
	gPar->Set<int>("ui_style_type", index);
	UpdateUIStyle();
}

void cPreferencesDialog::on_comboBox_ui_skin_currentIndexChanged(int index)
{
	if(!initFinished) return;
	gPar->Set<int>("ui_skin", index);
	UpdateUISkin();
}

