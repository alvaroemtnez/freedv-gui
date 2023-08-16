/*
   voicekeyer.cpp
   
   Voice Keyer implementation
*/

#include "main.h"

extern SNDFILE            *g_sfRecMicFile;
bool                g_recVoiceKeyerFile;
extern bool g_voice_keyer_tx;
extern wxMutex g_mutexProtectingCallbackData;

void MainFrame::OnTogBtnVoiceKeyerClick (wxCommandEvent& event)
{
    // If recording a new VK file, stop doing that now.
    if (g_recVoiceKeyerFile)
    {       
        g_mutexProtectingCallbackData.Lock();
        g_recVoiceKeyerFile = false;
        sf_close(g_sfRecMicFile);
        g_sfRecMicFile = nullptr;
        SetStatusText(wxT(""));
        g_mutexProtectingCallbackData.Unlock();
        
        m_togBtnAnalog->Enable(true);
        m_togBtnVoiceKeyer->SetValue(false);
        m_togBtnVoiceKeyer->SetBackgroundColour(wxNullColour);
    }
    else
    {
        bool enableVK = false;
        
        if (vk_state == VK_IDLE)
        {
            m_togBtnVoiceKeyer->SetValue(true);
            VoiceKeyerProcessEvent(VK_START);
            enableVK = true;
        }
        else
            VoiceKeyerProcessEvent(VK_SPACE_BAR);
        
        wxColour vkBackgroundColor(55, 155, 175);
        m_togBtnVoiceKeyer->SetBackgroundColour(enableVK ? vkBackgroundColor : wxNullColour);
    }

    event.Skip();
}

void MainFrame::OnRecordNewVoiceKeyerFile( wxCommandEvent& event )
{
    wxFileDialog saveFileDialog(
        this,
        wxT("Select Voice Keyer File"),
        wxGetApp().appConfiguration.playFileToMicInPath,
        wxEmptyString,
        wxT("WAV files (*.wav)|*.wav|")
        wxT("All files (*.*)|*.*"),
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT
        );
        
    if(saveFileDialog.ShowModal() == wxID_CANCEL)
    {
        return;     // the user changed their mind...
    }
    
    // The below code ensures that the last folder the above dialog was
    // navigated to persists across executions.
    wxString soundFile = saveFileDialog.GetPath();
    wxString tmpString = wxGetApp().appConfiguration.playFileToMicInPath;
    wxString extension;
    wxFileName::SplitPath(soundFile, &tmpString, nullptr, &extension);
    wxGetApp().appConfiguration.playFileToMicInPath = tmpString;
    
    // Append .wav extension to the end if needed.
    if (extension.Lower() != _("wav"))
    {
        soundFile += ".wav";
    }
    
    int sample_rate = wxGetApp().appConfiguration.audioConfiguration.soundCard2In.sampleRate;
    SF_INFO     sfInfo;
    
    sfInfo.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    sfInfo.channels   = 1;
    sfInfo.samplerate = sample_rate;

    g_sfRecMicFile = sf_open(soundFile.c_str(), SFM_WRITE, &sfInfo);
    if(g_sfRecMicFile == NULL)
    {
        wxString strErr = sf_strerror(NULL);
        wxMessageBox(strErr, wxT("Couldn't open sound file"), wxOK);
        return;
    }

    SetStatusText(wxT("Recording file ") + soundFile + wxT(" from microphone") , 0);
    g_recVoiceKeyerFile = true;
    vkFileName_ = soundFile;
    
    // Disable Analog and VK buttons while recording is happening
    m_togBtnAnalog->Enable(false);
    m_togBtnVoiceKeyer->SetValue(true);
    m_togBtnVoiceKeyer->SetBackgroundColour(*wxRED);
}

void MainFrame::OnChooseAlternateVoiceKeyerFile( wxCommandEvent& event )
{
    wxFileDialog openFileDialog(
        this,
        wxT("Select Voice Keyer File"),
        wxGetApp().appConfiguration.playFileToMicInPath,
        wxEmptyString,
        wxT("WAV files (*.wav)|*.wav|")
        wxT("All files (*.*)|*.*"),
        wxFD_OPEN | wxFD_FILE_MUST_EXIST
        );

    if(openFileDialog.ShowModal() == wxID_CANCEL)
    {
        return;     // the user changed their mind...
    }

    // The below code ensures that the last folder the above dialog was
    // navigated to persists across executions.
    wxString tmpString = wxGetApp().appConfiguration.playFileToMicInPath;
    wxString soundFile = openFileDialog.GetPath();
    wxFileName::SplitPath(soundFile, &tmpString, nullptr, nullptr);
    wxGetApp().appConfiguration.playFileToMicInPath = tmpString;
    
    vkFileName_ = soundFile;
}

void MainFrame::OnTogBtnVoiceKeyerRightClick( wxContextMenuEvent& event )
{
    // Only handle right-click if idle
    if (vk_state == VK_IDLE && !m_btnTogPTT->GetValue())
    {
        auto sz = m_togBtnVoiceKeyer->GetSize();
        m_togBtnVoiceKeyer->PopupMenu(voiceKeyerPopupMenu_, wxPoint(-sz.GetWidth() - 25, 0));
    }
}

void MainFrame::OnSetMonitorVKAudio( wxCommandEvent& event )
{
    wxGetApp().appConfiguration.monitorVoiceKeyerAudio = event.IsChecked();
}

extern SNDFILE *g_sfPlayFile;
extern bool g_playFileToMicIn;
extern bool g_loopPlayFileToMicIn;
extern FreeDVInterface freedvInterface;
extern int g_sfTxFs;

int MainFrame::VoiceKeyerStartTx(void)
{
    int next_state;

    // start playing wave file or die trying

    SF_INFO sfInfo;
    sfInfo.format = 0;

    SNDFILE* tmpPlayFile = sf_open(vkFileName_.c_str(), SFM_READ, &sfInfo);
    if(tmpPlayFile == NULL) {
        wxString strErr = sf_strerror(NULL);
        wxMessageBox(strErr, wxT("Couldn't open:") + vkFileName_, wxOK);
        next_state = VK_IDLE;
        m_togBtnVoiceKeyer->SetBackgroundColour(wxNullColour);
        m_togBtnVoiceKeyer->SetValue(false);
    }
    else {
        g_sfTxFs = sfInfo.samplerate;
        g_sfPlayFile = tmpPlayFile;
        
        SetStatusText(wxT("Voice Keyer: Playing file ") + vkFileName_ + wxT(" to mic input") , 0);
        g_loopPlayFileToMicIn = false;
        g_playFileToMicIn = true;

        m_btnTogPTT->SetValue(true); togglePTT();
        next_state = VK_TX;

        if (wxGetApp().appConfiguration.monitorVoiceKeyerAudio)
        {
            g_voice_keyer_tx = true;
        }
    }

    return next_state;
}


void MainFrame::VoiceKeyerProcessEvent(int vk_event) {
    int next_state = vk_state;
    
    switch(vk_state) {

    case VK_IDLE:
        g_voice_keyer_tx = false;

        if (vk_event == VK_START) {
            // sample these puppies at start just in case they are changed while VK running
            vk_rx_pause = wxGetApp().appConfiguration.voiceKeyerRxPause;
            vk_repeats = wxGetApp().appConfiguration.voiceKeyerRepeats;
            if (g_verbose) fprintf(stderr, "vk_rx_pause: %d vk_repeats: %d\n", vk_rx_pause, vk_repeats);

            vk_repeat_counter = 0;
            next_state = VoiceKeyerStartTx();
        }
        break;

     case VK_TX:

        // In this state we are transmitting and playing a wave file
        // to Mic In

        if (vk_event == VK_SPACE_BAR) {
            m_btnTogPTT->SetValue(false); 
            m_btnTogPTT->SetBackgroundColour(wxNullColour);
            togglePTT();
            m_togBtnVoiceKeyer->SetValue(false);
            m_togBtnVoiceKeyer->SetBackgroundColour(wxNullColour);
            next_state = VK_IDLE;
            CallAfter([&]() { StopPlayFileToMicIn(); });
        }

        if (vk_event == VK_PLAY_FINISHED) {
            m_btnTogPTT->SetValue(false); 
            m_btnTogPTT->SetBackgroundColour(wxNullColour);
            togglePTT();
            vk_repeat_counter++;
            if (vk_repeat_counter > vk_repeats) {
                m_togBtnVoiceKeyer->SetValue(false);
                m_togBtnVoiceKeyer->SetBackgroundColour(wxNullColour);
                next_state = VK_IDLE;
            }
            else {
                vk_rx_time = 0.0;
                next_state = VK_RX;
            }
        }

        break;

     case VK_RX:
        g_voice_keyer_tx = false;

        // in this state we are receiving and waiting for
        // delay timer or valid sync

        if (vk_event == VK_DT) {
            if (freedvInterface.getSync() == 1) {
                // if we detect sync transition to SYNC_WAIT state
                next_state = VK_SYNC_WAIT;
                vk_rx_sync_time = 0.0;
            } else {
                vk_rx_time += DT;
                if (vk_rx_time >= vk_rx_pause) {
                    next_state = VoiceKeyerStartTx();
                }
            }
        }

        if (vk_event == VK_SPACE_BAR) {
            m_togBtnVoiceKeyer->SetValue(false);
            m_togBtnVoiceKeyer->SetBackgroundColour(wxNullColour);
            next_state = VK_IDLE;
        }

        break;

     case VK_SYNC_WAIT:
        g_voice_keyer_tx = false;

        // In this state we wait for valid sync to last
        // VK_SYNC_WAIT_TIME seconds

        if (vk_event == VK_SPACE_BAR) {
            m_togBtnVoiceKeyer->SetValue(false);
            m_togBtnVoiceKeyer->SetBackgroundColour(wxNullColour);
            next_state = VK_IDLE;
        }

        if (vk_event == VK_DT) {
            if (freedvInterface.getSync() == 0) {
                // if we lose sync transition to RX State
                next_state = VK_RX;
            } else {
                vk_rx_time += DT;
                vk_rx_sync_time += DT;
            }

            // drop out of voice keyer if we get a few seconds of valid sync

            if (vk_rx_sync_time >= VK_SYNC_WAIT_TIME) {
                m_togBtnVoiceKeyer->SetValue(false);
                m_togBtnVoiceKeyer->SetBackgroundColour(wxNullColour);
                next_state = VK_IDLE;
            }
        }
        break;

    default:
        // catch anything we missed

        m_btnTogPTT->SetValue(false); 
        m_btnTogPTT->SetBackgroundColour(wxNullColour);
        togglePTT();
        m_togBtnVoiceKeyer->SetValue(false);
        m_togBtnVoiceKeyer->SetBackgroundColour(wxNullColour);
        next_state = VK_IDLE;
        g_voice_keyer_tx = false;
    }

    //if ((vk_event != VK_DT) || (vk_state != next_state))
    //    fprintf(stderr, "VoiceKeyerProcessEvent: vk_state: %d vk_event: %d next_state: %d  vk_repeat_counter: %d\n", vk_state, vk_event, next_state, vk_repeat_counter);
    vk_state = next_state;
}

