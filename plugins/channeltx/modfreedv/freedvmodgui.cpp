///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QDockWidget>
#include <QMainWindow>
#include <QFileDialog>
#include <QTime>
#include <QDebug>

#include "freedvmodgui.h"

#include "device/devicesinkapi.h"
#include "device/deviceuiset.h"
#include "dsp/spectrumvis.h"
#include "ui_freedvmodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/basicchannelsettingsdialog.h"
#include "mainwindow.h"

FreeDVModGUI* FreeDVModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    FreeDVModGUI* gui = new FreeDVModGUI(pluginAPI, deviceUISet, channelTx);
	return gui;
}

void FreeDVModGUI::destroy()
{
    delete this;
}

void FreeDVModGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString FreeDVModGUI::getName() const
{
	return objectName();
}

qint64 FreeDVModGUI::getCenterFrequency() const {
	return m_channelMarker.getCenterFrequency();
}

void FreeDVModGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void FreeDVModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
}

QByteArray FreeDVModGUI::serialize() const
{
    return m_settings.serialize();
}

bool FreeDVModGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        qDebug("FreeDVModGUI::deserialize");
        displaySettings();
        applyBandwidths(5 - ui->spanLog2->value(), true); // does applySettings(true)
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        displaySettings();
        applyBandwidths(5 - ui->spanLog2->value(), true); // does applySettings(true)
        return false;
    }
}

bool FreeDVModGUI::handleMessage(const Message& message)
{
    if (FreeDVMod::MsgReportFileSourceStreamData::match(message))
    {
        m_recordSampleRate = ((FreeDVMod::MsgReportFileSourceStreamData&)message).getSampleRate();
        m_recordLength = ((FreeDVMod::MsgReportFileSourceStreamData&)message).getRecordLength();
        m_samplesCount = 0;
        updateWithStreamData();
        return true;
    }
    else if (FreeDVMod::MsgReportFileSourceStreamTiming::match(message))
    {
        m_samplesCount = ((FreeDVMod::MsgReportFileSourceStreamTiming&)message).getSamplesCount();
        updateWithStreamTime();
        return true;
    }
    else if (DSPConfigureAudio::match(message))
    {
        qDebug("FreeDVModGUI::handleMessage: DSPConfigureAudio: %d", m_freeDVMod->getAudioSampleRate());
        applyBandwidths(5 - ui->spanLog2->value()); // will update spectrum details with new sample rate
        return true;
    }
    else if (FreeDVMod::MsgConfigureFreeDVMod::match(message))
    {
        const FreeDVMod::MsgConfigureFreeDVMod& cfg = (FreeDVMod::MsgConfigureFreeDVMod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (CWKeyer::MsgConfigureCWKeyer::match(message))
    {
        const CWKeyer::MsgConfigureCWKeyer& cfg = (CWKeyer::MsgConfigureCWKeyer&) message;
        ui->cwKeyerGUI->displaySettings(cfg.getSettings());
        return true;
    }
    else
    {
        return false;
    }
}

void FreeDVModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void FreeDVModGUI::channelMarkerUpdate()
{
    m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
    displaySettings();
    applySettings();
}

void FreeDVModGUI::handleSourceMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void FreeDVModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void FreeDVModGUI::on_flipSidebands_clicked(bool checked)
{
    (void) checked;
    int bwValue = ui->BW->value();
    int lcValue = ui->lowCut->value();
    ui->BW->setValue(-bwValue);
    ui->lowCut->setValue(-lcValue);
}

void FreeDVModGUI::on_dsb_toggled(bool dsb)
{
    ui->flipSidebands->setEnabled(!dsb);
    applyBandwidths(5 - ui->spanLog2->value());
}

void FreeDVModGUI::on_audioBinaural_toggled(bool checked)
{
    m_settings.m_audioBinaural = checked;
	applySettings();
}

void FreeDVModGUI::on_audioFlipChannels_toggled(bool checked)
{
    m_settings.m_audioFlipChannels = checked;
	applySettings();
}

void FreeDVModGUI::on_spanLog2_valueChanged(int value)
{
    if ((value < 0) || (value > 4)) {
        return;
    }

    applyBandwidths(5 - value);
}

void FreeDVModGUI::on_BW_valueChanged(int value)
{
    (void) value;
    applyBandwidths(5 - ui->spanLog2->value());
}

void FreeDVModGUI::on_lowCut_valueChanged(int value)
{
    (void) value;
    applyBandwidths(5 - ui->spanLog2->value());
}

void FreeDVModGUI::on_toneFrequency_valueChanged(int value)
{
    ui->toneFrequencyText->setText(QString("%1k").arg(value / 100.0, 0, 'f', 2));
    m_settings.m_toneFrequency = value * 10.0;
    applySettings();
}

void FreeDVModGUI::on_volume_valueChanged(int value)
{
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_volumeFactor = value / 10.0;
    applySettings();
}

void FreeDVModGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
	applySettings();
}

void FreeDVModGUI::on_playLoop_toggled(bool checked)
{
    m_settings.m_playLoop = checked;
    applySettings();
}

void FreeDVModGUI::on_play_toggled(bool checked)
{
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    ui->mic->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? FreeDVModSettings::FreeDVModInputFile : FreeDVModSettings::FreeDVModInputNone;
    applySettings();
    ui->navTimeSlider->setEnabled(!checked);
    m_enableNavTime = !checked;
}

void FreeDVModGUI::on_tone_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    ui->mic->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? FreeDVModSettings::FreeDVModInputTone : FreeDVModSettings::FreeDVModInputNone;
    applySettings();
}

void FreeDVModGUI::on_morseKeyer_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->mic->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? FreeDVModSettings::FreeDVModInputCWTone : FreeDVModSettings::FreeDVModInputNone;
    applySettings();
}

void FreeDVModGUI::on_mic_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    ui->tone->setEnabled(!checked); // release other source inputs
    m_settings.m_modAFInput = checked ? FreeDVModSettings::FreeDVModInputAudio : FreeDVModSettings::FreeDVModInputNone;
    applySettings();
}

void FreeDVModGUI::on_agc_toggled(bool checked)
{
    m_settings.m_agc = checked;
    applySettings();
}

void FreeDVModGUI::on_agcOrder_valueChanged(int value){
    QString s = QString::number(value / 100.0, 'f', 2);
    ui->agcOrderText->setText(s);
    m_settings.m_agcOrder = value / 100.0;
    applySettings();
}

void FreeDVModGUI::on_agcTime_valueChanged(int value){
    QString s = QString::number(FreeDVModSettings::getAGCTimeConstant(value), 'f', 0);
    ui->agcTimeText->setText(s);
    m_settings.m_agcTime = FreeDVModSettings::getAGCTimeConstant(value) * 48;
    applySettings();
}

void FreeDVModGUI::on_agcThreshold_valueChanged(int value)
{
    m_settings.m_agcThreshold = value; // dB
    displayAGCPowerThreshold();
    applySettings();
}

void FreeDVModGUI::on_agcThresholdGate_valueChanged(int value)
{
    QString s = QString::number(value, 'f', 0);
    ui->agcThresholdGateText->setText(s);
    m_settings.m_agcThresholdGate = value * 48;
    applySettings();
}

void FreeDVModGUI::on_agcThresholdDelay_valueChanged(int value)
{
    QString s = QString::number(value * 10, 'f', 0);
    ui->agcThresholdDelayText->setText(s);
    m_settings.m_agcThresholdDelay = value * 480;
    applySettings();
}

void FreeDVModGUI::on_navTimeSlider_valueChanged(int value)
{
    if (m_enableNavTime && ((value >= 0) && (value <= 100)))
    {
        int t_sec = (m_recordLength * value) / 100;
        QTime t(0, 0, 0, 0);
        t = t.addSecs(t_sec);

        FreeDVMod::MsgConfigureFileSourceSeek* message = FreeDVMod::MsgConfigureFileSourceSeek::create(value);
        m_freeDVMod->getInputMessageQueue()->push(message);
    }
}

void FreeDVModGUI::on_showFileDialog_clicked(bool checked)
{
    (void) checked;
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open raw audio file"), ".", tr("Raw audio Files (*.raw)"), 0, QFileDialog::DontUseNativeDialog);

    if (fileName != "")
    {
        m_fileName = fileName;
        ui->recordFileText->setText(m_fileName);
        ui->play->setEnabled(true);
        configureFileName();
    }
}

void FreeDVModGUI::configureFileName()
{
    qDebug() << "FileSourceGui::configureFileName: " << m_fileName.toStdString().c_str();
    FreeDVMod::MsgConfigureFileSourceName* message = FreeDVMod::MsgConfigureFileSourceName::create(m_fileName);
    m_freeDVMod->getInputMessageQueue()->push(message);
}

void FreeDVModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
}

void FreeDVModGUI::onMenuDialogCalled(const QPoint &p)
{
    BasicChannelSettingsDialog dialog(&m_channelMarker, this);
    dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
    dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
    dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
    dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
    dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);

    dialog.move(p);
    dialog.exec();

    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
    m_settings.m_title = m_channelMarker.getTitle();
    m_settings.m_useReverseAPI = dialog.useReverseAPI();
    m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
    m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
    m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
    m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

    setWindowTitle(m_settings.m_title);
    setTitleColor(m_settings.m_rgbColor);

    applySettings();
}

FreeDVModGUI::FreeDVModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::FreeDVModGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
	m_doApplySettings(true),
	m_spectrumRate(6000),
    m_recordLength(0),
    m_recordSampleRate(48000),
    m_samplesCount(0),
    m_tickCount(0),
    m_enableNavTime(false)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_spectrumVis = new SpectrumVis(SDR_TX_SCALEF, ui->glSpectrum);
	m_freeDVMod = (FreeDVMod*) channelTx;
	m_freeDVMod->setSpectrumSampleSink(m_spectrumVis);
	m_freeDVMod->setMessageQueueToGUI(getInputMessageQueue());

    resetToDefaults();

	ui->glSpectrum->setCenterFrequency(m_spectrumRate/2);
	ui->glSpectrum->setSampleRate(m_spectrumRate);
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
	ui->glSpectrum->setSsbSpectrum(true);
	ui->glSpectrum->connectTimer(MainWindow::getInstance()->getMasterTimer());

	connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->mic);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
	m_channelMarker.setColor(Qt::darkCyan);
	m_channelMarker.setBandwidth(m_spectrumRate);
	m_channelMarker.setSidebands(ChannelMarker::usb);
	m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("FreeDV Modulator");
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true);

    setTitleColor(m_channelMarker.getColor());

    m_deviceUISet->registerTxChannelInstance(FreeDVMod::m_channelIdURI, this);
    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_deviceUISet->addRollupWidget(this);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    ui->cwKeyerGUI->setBuddies(m_freeDVMod->getInputMessageQueue(), m_freeDVMod->getCWKeyer());
    ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_settings.setCWKeyerGUI(ui->cwKeyerGUI);

	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
	connect(m_freeDVMod, SIGNAL(levelChanged(qreal, qreal, int)), ui->volumeMeter, SLOT(levelChanged(qreal, qreal, int)));

    m_iconDSBUSB.addPixmap(QPixmap("://dsb.png"), QIcon::Normal, QIcon::On);
    m_iconDSBUSB.addPixmap(QPixmap("://usb.png"), QIcon::Normal, QIcon::Off);
    m_iconDSBLSB.addPixmap(QPixmap("://dsb.png"), QIcon::Normal, QIcon::On);
    m_iconDSBLSB.addPixmap(QPixmap("://lsb.png"), QIcon::Normal, QIcon::Off);

    displaySettings();
    applyBandwidths(5 - ui->spanLog2->value(), true); // does applySettings(true)
}

FreeDVModGUI::~FreeDVModGUI()
{
    m_deviceUISet->removeTxChannelInstance(this);
	delete m_freeDVMod; // TODO: check this: when the GUI closes it has to delete the modulator
	delete m_spectrumVis;
	delete ui;
}

bool FreeDVModGUI::blockApplySettings(bool block)
{
    bool ret = !m_doApplySettings;
    m_doApplySettings = !block;
    return ret;
}

void FreeDVModGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		FreeDVMod::MsgConfigureChannelizer *msgChan = FreeDVMod::MsgConfigureChannelizer::create(
		        48000, m_settings.m_inputFrequencyOffset);
        m_freeDVMod->getInputMessageQueue()->push(msgChan);

		FreeDVMod::MsgConfigureFreeDVMod *msg = FreeDVMod::MsgConfigureFreeDVMod::create(m_settings, force);
		m_freeDVMod->getInputMessageQueue()->push(msg);
	}
}

void FreeDVModGUI::applyBandwidths(int spanLog2, bool force)
{
    bool dsb = ui->dsb->isChecked();
    m_spectrumRate = m_freeDVMod->getAudioSampleRate() / (1<<spanLog2);
    int bw = ui->BW->value();
    int lw = ui->lowCut->value();
    int bwMax = m_freeDVMod->getAudioSampleRate() / (100*(1<<spanLog2));
    int tickInterval = m_spectrumRate / 1200;
    tickInterval = tickInterval == 0 ? 1 : tickInterval;

    qDebug() << "FreeDVModGUI::applyBandwidths:"
            << " dsb: " << dsb
            << " spanLog2: " << spanLog2
            << " m_spectrumRate: " << m_spectrumRate
            << " bw: " << bw
            << " lw: " << lw
            << " bwMax: " << bwMax
            << " tickInterval: " << tickInterval;

    ui->BW->setTickInterval(tickInterval);
    ui->lowCut->setTickInterval(tickInterval);

    bw = bw < -bwMax ? -bwMax : bw > bwMax ? bwMax : bw;

    if (bw < 0) {
        lw = lw < bw+1 ? bw+1 : lw < 0 ? lw : 0;
    } else if (bw > 0) {
        lw = lw > bw-1 ? bw-1 : lw < 0 ? 0 : lw;
    } else {
        lw = 0;
    }

    if (dsb)
    {
        bw = bw < 0 ? -bw : bw;
        lw = 0;
    }

    QString spanStr = QString::number(bwMax/10.0, 'f', 1);
    QString bwStr   = QString::number(bw/10.0, 'f', 1);
    QString lwStr   = QString::number(lw/10.0, 'f', 1);

    if (dsb)
    {
        ui->BWText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(bwStr));
        ui->spanText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(spanStr));
        ui->scaleMinus->setText("0");
        ui->scaleCenter->setText("");
        ui->scalePlus->setText(tr("%1").arg(QChar(0xB1, 0x00)));
        ui->lsbLabel->setText("");
        ui->usbLabel->setText("");
        ui->glSpectrum->setCenterFrequency(0);
        ui->glSpectrum->setSampleRate(2*m_spectrumRate);
        ui->glSpectrum->setSsbSpectrum(false);
        ui->glSpectrum->setLsbDisplay(false);
    }
    else
    {
        ui->BWText->setText(tr("%1k").arg(bwStr));
        ui->spanText->setText(tr("%1k").arg(spanStr));
        ui->scaleMinus->setText("-");
        ui->scaleCenter->setText("0");
        ui->scalePlus->setText("+");
        ui->lsbLabel->setText("LSB");
        ui->usbLabel->setText("USB");
        ui->glSpectrum->setCenterFrequency(m_spectrumRate/2);
        ui->glSpectrum->setSampleRate(m_spectrumRate);
        ui->glSpectrum->setSsbSpectrum(true);
        ui->glSpectrum->setLsbDisplay(bw < 0);
    }

    ui->lowCutText->setText(tr("%1k").arg(lwStr));


    ui->BW->blockSignals(true);
    ui->lowCut->blockSignals(true);

    ui->BW->setMaximum(bwMax);
    ui->BW->setMinimum(dsb ? 0 : -bwMax);
    ui->BW->setValue(bw);

    ui->lowCut->setMaximum(dsb ? 0 :  bwMax);
    ui->lowCut->setMinimum(dsb ? 0 : -bwMax);
    ui->lowCut->setValue(lw);

    ui->lowCut->blockSignals(false);
    ui->BW->blockSignals(false);


    m_settings.m_dsb = dsb;
    m_settings.m_spanLog2 = spanLog2;
    m_settings.m_bandwidth = bw * 100;
    m_settings.m_lowCutoff = lw * 100;

    applySettings(force);

    bool applySettingsWereBlocked = blockApplySettings(true);
    m_channelMarker.setBandwidth(bw * 200);
    m_channelMarker.setSidebands(dsb ? ChannelMarker::dsb : bw < 0 ? ChannelMarker::lsb : ChannelMarker::usb);
    ui->dsb->setIcon(bw < 0 ? m_iconDSBLSB : m_iconDSBUSB);
    if (!dsb) { m_channelMarker.setLowCutoff(lw * 100); }
    blockApplySettings(applySettingsWereBlocked);
}

void FreeDVModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_settings.m_bandwidth * 2);
    m_channelMarker.setLowCutoff(m_settings.m_lowCutoff);

    ui->flipSidebands->setEnabled(!m_settings.m_dsb);

    if (m_settings.m_dsb) {
        m_channelMarker.setSidebands(ChannelMarker::dsb);
    } else {
        if (m_settings.m_bandwidth < 0) {
            m_channelMarker.setSidebands(ChannelMarker::lsb);
            ui->dsb->setIcon(m_iconDSBLSB);
        } else {
            m_channelMarker.setSidebands(ChannelMarker::usb);
            ui->dsb->setIcon(m_iconDSBUSB);
        }
    }

    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    QString s = QString::number(m_settings.m_agcTime / 48, 'f', 0);
    ui->agcTimeText->setText(s);
    ui->agcTime->setValue(FreeDVModSettings::getAGCTimeConstantIndex(m_settings.m_agcTime / 48));
    displayAGCPowerThreshold();
    s = QString::number(m_settings.m_agcThresholdGate / 48, 'f', 0);
    ui->agcThresholdGateText->setText(s);
    ui->agcThresholdGate->setValue(m_settings.m_agcThresholdGate / 48);
    s = QString::number(m_settings.m_agcThresholdDelay / 48, 'f', 0);
    ui->agcThresholdDelayText->setText(s);
    ui->agcThresholdDelay->setValue(m_settings.m_agcThresholdDelay / 480);
    s = QString::number(m_settings.m_agcOrder, 'f', 2);
    ui->agcOrderText->setText(s);
    ui->agcOrder->setValue(roundf(m_settings.m_agcOrder * 100.0));

    ui->agc->setChecked(m_settings.m_agc);
    ui->audioBinaural->setChecked(m_settings.m_audioBinaural);
    ui->audioFlipChannels->setChecked(m_settings.m_audioFlipChannels);
    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->playLoop->setChecked(m_settings.m_playLoop);

    // Prevent uncontrolled triggering of applyBandwidths
    ui->spanLog2->blockSignals(true);
    ui->dsb->blockSignals(true);
    ui->BW->blockSignals(true);

    ui->dsb->setChecked(m_settings.m_dsb);
    ui->spanLog2->setValue(5 - m_settings.m_spanLog2);

    ui->BW->setValue(roundf(m_settings.m_bandwidth/100.0));
    s = QString::number(m_settings.m_bandwidth/1000.0, 'f', 1);

    if (m_settings.m_dsb)
    {
        ui->BWText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(s));
    }
    else
    {
        ui->BWText->setText(tr("%1k").arg(s));
    }

    ui->spanLog2->blockSignals(false);
    ui->dsb->blockSignals(false);
    ui->BW->blockSignals(false);

    // The only one of the four signals triggering applyBandwidths will trigger it once only with all other values
    // set correctly and therefore validate the settings and apply them to dependent widgets
    ui->lowCut->setValue(m_settings.m_lowCutoff / 100.0);
    ui->lowCutText->setText(tr("%1k").arg(m_settings.m_lowCutoff / 1000.0));

    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);

    ui->toneFrequency->setValue(roundf(m_settings.m_toneFrequency / 10.0));
    ui->toneFrequencyText->setText(QString("%1k").arg(m_settings.m_toneFrequency / 1000.0, 0, 'f', 2));

    ui->volume->setValue(m_settings.m_volumeFactor * 10.0);
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volumeFactor, 0, 'f', 1));

    ui->tone->setEnabled((m_settings.m_modAFInput == FreeDVModSettings::FreeDVModInputAF::FreeDVModInputTone)
            || (m_settings.m_modAFInput == FreeDVModSettings::FreeDVModInputAF::FreeDVModInputNone));
    ui->mic->setEnabled((m_settings.m_modAFInput == FreeDVModSettings::FreeDVModInputAF::FreeDVModInputAudio)
            || (m_settings.m_modAFInput == FreeDVModSettings::FreeDVModInputAF::FreeDVModInputNone));
    ui->play->setEnabled((m_settings.m_modAFInput == FreeDVModSettings::FreeDVModInputAF::FreeDVModInputFile)
            || (m_settings.m_modAFInput == FreeDVModSettings::FreeDVModInputAF::FreeDVModInputNone));
    ui->morseKeyer->setEnabled((m_settings.m_modAFInput == FreeDVModSettings::FreeDVModInputAF::FreeDVModInputCWTone)
            || (m_settings.m_modAFInput == FreeDVModSettings::FreeDVModInputAF::FreeDVModInputNone));

    ui->tone->setChecked(m_settings.m_modAFInput == FreeDVModSettings::FreeDVModInputAF::FreeDVModInputTone);
    ui->mic->setChecked(m_settings.m_modAFInput == FreeDVModSettings::FreeDVModInputAF::FreeDVModInputAudio);
    ui->play->setChecked(m_settings.m_modAFInput == FreeDVModSettings::FreeDVModInputAF::FreeDVModInputFile);
    ui->morseKeyer->setChecked(m_settings.m_modAFInput == FreeDVModSettings::FreeDVModInputAF::FreeDVModInputCWTone);

    blockApplySettings(false);
}

void FreeDVModGUI::displayAGCPowerThreshold()
{
    if (m_settings.m_agcThreshold == -99)
    {
        ui->agcThresholdText->setText("---");
    }
    else
    {
        QString s = QString::number(m_settings.m_agcThreshold, 'f', 0);
        ui->agcThresholdText->setText(s);
    }

    ui->agcThreshold->setValue(m_settings.m_agcThreshold);
}

void FreeDVModGUI::leaveEvent(QEvent*)
{
	m_channelMarker.setHighlighted(false);
}

void FreeDVModGUI::enterEvent(QEvent*)
{
	m_channelMarker.setHighlighted(true);
}

void FreeDVModGUI::audioSelect()
{
    qDebug("FreeDVModGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName, true); // true for input
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void FreeDVModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_freeDVMod->getMagSq());
	m_channelPowerDbAvg(powDb);
	ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));

    if (((++m_tickCount & 0xf) == 0) && (m_settings.m_modAFInput == FreeDVModSettings::FreeDVModInputFile))
    {
        FreeDVMod::MsgConfigureFileSourceStreamTiming* message = FreeDVMod::MsgConfigureFileSourceStreamTiming::create();
        m_freeDVMod->getInputMessageQueue()->push(message);
    }
}

void FreeDVModGUI::updateWithStreamData()
{
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(m_recordLength);
    QString s_time = recordLength.toString("HH:mm:ss");
    ui->recordLengthText->setText(s_time);
    updateWithStreamTime();
}

void FreeDVModGUI::updateWithStreamTime()
{
    int t_sec = 0;
    int t_msec = 0;

    if (m_recordSampleRate > 0)
    {
        t_msec = ((m_samplesCount * 1000) / m_recordSampleRate) % 1000;
        t_sec = m_samplesCount / m_recordSampleRate;
    }

    QTime t(0, 0, 0, 0);
    t = t.addSecs(t_sec);
    t = t.addMSecs(t_msec);
    QString s_timems = t.toString("HH:mm:ss.zzz");
    QString s_time = t.toString("HH:mm:ss");
    ui->relTimeText->setText(s_timems);

    if (!m_enableNavTime)
    {
        float posRatio = (float) t_sec / (float) m_recordLength;
        ui->navTimeSlider->setValue((int) (posRatio * 100.0));
    }
}