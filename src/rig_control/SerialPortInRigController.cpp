//=========================================================================
// Name:            SerialPortInRigController.cpp
// Purpose:         Monitors PTT input using serial port.
//
// Authors:         Mooneer Salem
// License:
//
//  All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//=========================================================================

#include "SerialPortInRigController.h"

#define PTT_INPUT_MONITORING_TIME_MS 10

SerialPortInRigController::SerialPortInRigController(std::string serialPort, bool ctsPos)
    : SerialPortRigController(serialPort)
    , threadExiting_(false)
    , ctsPos_(ctsPos)
    , currentPttInputState_(false)
{
    // Perform required initialization on successful connection.
    onRigConnected += [&](IRigController*) {
        // Set RTS and DTR enabled for certain PTT input interfaces.
        raiseRTS_();
        raiseDTR_();

        // Start polling thread.
        threadExiting_ = false;
        currentPttInputState_ = false;
        pollThread_ = std::thread(std::bind(&SerialPortInRigController::pollThreadEntry_, this));
    };

    onRigDisconnected += [&](IRigController*) {
        threadExiting_ = true;
        pollThread_.join();
    };
}

bool SerialPortInRigController::getCTS_() 
{
    if(serialPortHandle_ == COM_HANDLE_INVALID)
        return false;
#ifdef _WIN32
    DWORD modemFlags = 0;
    GetCommModemStatus(serialPortHandle_, &modemFlags);
    return (modemFlags & MS_CTS_ON);
#else
    {    // For C89 happiness
        int flags = 0;
        ioctl(serialPortHandle_, TIOCMGET, &flags);
        return flags & TIOCM_CTS;
    }
#endif    
}

void SerialPortInRigController::pollThreadEntry_()
{
    while (!threadExiting_)
    {
        enqueue_(std::bind(&SerialPortInRigController::pollSerialPort_, this));
        std::this_thread::sleep_for(std::chrono::milliseconds(PTT_INPUT_MONITORING_TIME_MS));
    }
}

void SerialPortInRigController::pollSerialPort_()
{
    bool pttState = getCTS_();
    if (pttState != currentPttInputState_)
    {
        currentPttInputState_ = pttState;
        onPttChange(this, currentPttInputState_ == ctsPos_);
    }
}
