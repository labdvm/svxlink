/**
@file	 TetraLogic.h
@brief   Contains a Tetra logic SvxLink core implementation
@author  Tobias Blomberg / SM0SVX & Adi Bier / DL1HRC
@date	 2020-05-27

\verbatim
SvxLink - A Multi Purpose Voice Services System for Ham Radio Use
Copyright (C) 2003-2020 Tobias Blomberg / SM0SVX

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\endverbatim
*/


#ifndef TETRA_LOGIC_INCLUDED
#define TETRA_LOGIC_INCLUDED


/****************************************************************************
 *
 * System Includes
 *
 ****************************************************************************/

#include <string>
#include <vector>
#include <list>
#include <time.h>


/****************************************************************************
 *
 * Project Includes
 *
 ****************************************************************************/

 #include <AsyncSerial.h>


/****************************************************************************
 *
 * Local Includes
 *
 ****************************************************************************/

#include "Logic.h"


/****************************************************************************
 *
 * Forward declarations
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Namespace
 *
 ****************************************************************************/

//namespace MyNameSpace
//{


/****************************************************************************
 *
 * Forward declarations of classes inside of the declared namespace
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Defines & typedefs
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Exported Global Variables
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Class definitions
 *
 ****************************************************************************/

/**
@brief	This class implements a Tetra logic core
@author Adi Bier
@date   2020-05-27
*/
class TetraLogic : public Logic
{
  public:
    /**
     * @brief 	Constuctor
     * @param 	cfg A previously opened configuration
     * @param 	name The configuration section name of this logic
     */
    TetraLogic(Async::Config &cfg, const std::string &name);

    /**
     * @brief 	Destructor
     */
    ~TetraLogic(void);

    /**
     * @brief 	Initialize the Tetra logic core
     * @return	Returns \em true if the initialization was successful or else
     *	      	\em false is returned.
     */
    bool initialize(void);

  protected:
    virtual void audioStreamStateChange(bool is_active, bool is_idle);
    virtual void squelchOpen(bool is_open);
    virtual void transmitterStateChange(bool is_transmitting);
    virtual void allMsgsWritten(void);

  private:
    class Call;

    bool  mute_rx_on_tx;
    bool  mute_tx_on_rx;
    bool  rgr_sound_always;
    std::string   mcc;
    std::string   mnc;
    std::string   issi;
    int_fast16_t   gssi;
    std::string port;
    int32_t baudrate;
    std::string initstr;

    Async::Serial *pei;
    Async::Pty    *sds_pty;

    typedef std::vector<std::string> StrList;
    StrList initcmds;

    /*
     <CC instance >, <call status>, <AI service>,
     [<calling party identity type>], [<calling party identity>],
     [<hook>], [<simplex>], [<end to end encryption>],
     [<comms type>],
     [<slots/codec>], [<called party identity type>],
     [<called party identity>], [<priority level>]
    */
    struct Callinfo {
       int instance;
       int callstatus;
       int aistatus;
       int origin_cpit;
       int o_mcc;
       int o_mnc;
       int o_issi;
       int hook;
       int simplex;
       int e2eencryption;
       int commstype;
       int codec;
       int dest_cpit;
       int d_mcc;
       int d_mnc;
       int d_issi;
       int prio;
    };
    std::map<int, Callinfo> callinfo;

    struct QsoInfo {
      std::string tei;
      struct tm *start;
      struct tm *stop;
      std::list<std::string> members;
    };
    QsoInfo Qso;
    
    // contain a sds (state and message)
    struct Sds {
      std::string tei;
      std::string sds;
      std::string content;
      std::string message;
      struct tm *tos;
      int type; //STATE, LIP_SHORT,..
      int direction;  //INCOMING, OUTGOING
    };
    std::map<int, Sds> pending_sds;

     // contain user data
    struct User {
      int issi;
      std::string call;
      std::string name;
      std::string comment;
      float lat;
      float lon;
      std::string state;
      char aprs_sym;
      char aprs_tab;
      struct tm *last_activity;
    };
    std::map<std::string, User> userdata;

    int    peistate;
    std::string peistream;
    bool   debug;
    std::string aprspath;
    bool talkgroup_up;

    typedef enum
    {
      IDLE, CHECK_AT, INIT, IGNORE_ERRORS, INIT_COMPLETE, WAIT
    } Peidef;
    Peidef    peirequest;

    // AI Service
    // This parameter is used to determine the type of service to be used
    // in air interface call set up signalling. The services are all
    // defined in EN 300 392-2 [3] or EN 300 396-3 [25].
    typedef enum
    {
      TETRA_SPEECH=0, UNPROTECTED_DATA=1, PACKET_DATA=8, SDS_TYPE1=9,
      SDS_TYPE2=10, SDS_TYPE3=11, SDS_TYPE4=12, STATUS_SDS=13
    } TypeOfService;

    // direction of Sds
    typedef enum
    {
      OUTGOING, INCOMING
    } SdsDirection;
    
     // type of Sds
    typedef enum
    {
      STATE, TEXT, LIP_SHORT, COMPLEX_SDS_TL
    } SdsType;

    Async::Timer peiComTimer;
    Async::Timer peiActivityTimer;
    Async::Timer tgUpTimer;
    Call*    call;

    std::map<std::string, std::string> state_sds;
    StrList m_cmds;
    int pending_sdsid;
    char t_aprs_sym;
    char t_aprs_tab;

    void initPei(void);
    void onCharactersReceived(char *buf, int count);
    void sendPei(std::string cmd);
    void handleSds(std::string sds_head);
    void endCall(void);
    std::string handleTextSds(std::string m_message);
    void handleStateSds(std::string m_message);
    void handleTxGrant(std::string txgrant);
    std::string getTEI(std::string issi);
    int getNextVal(std::string &h);
    std::string getNextStr(std::string& h);
    void onComTimeout(Async::Timer *timer);
    void tgUpTimeout(Async::Timer *timer);
    void onPeiActivityTimeout(Async::Timer *timer);
    void initGroupCall(int gssi);
    void cfmSdsReceived(std::string tei);
    int handleMessage(std::string  m_message);
    void handleGroupcallBegin(std::string m_message);
    void handleGroupcallEnd(std::string m_message);
    void sdsPtyReceived(const void *buf, size_t count);
    std::string createAprsLip(std::string mesg);
    void handleCallEnd(std::string m_message);
    void getOpMode(std::string opmode);
    bool rmatch(std::string tok, std::string pattern);

};  /* class TetraLogic */


//} /* namespace */

#endif /* TETRA_LOGIC_INCLUDED */



/*
 * This file has not been truncated
 */

