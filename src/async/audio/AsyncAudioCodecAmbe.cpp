/**
@file	 AsyncAudioCodecAmbe.cpp
@brief   Contains a class to encode/decode ambe to/from voice
@author  Christian Stussak / Imaginary & Tobias Blomberg / SM0SVX
         & Adi Bier / DL1HRC
@date	 2017-07-10

\verbatim
Async - A library for programming event driven applications
Copyright (C) 2004-2017 Tobias Blomberg / SM0SVX

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


/****************************************************************************
 *
 * System Includes
 *
 ****************************************************************************/

#include <string>
#include <map>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <cassert>


/****************************************************************************
 *
 * Project Includes
 *
 ****************************************************************************/

#include <AsyncSerial.h>
#include <AsyncUdpSocket.h>
#include <AsyncIpAddress.h>
#include <AsyncDnsLookup.h>


/****************************************************************************
 *
 * Local Includes
 *
 ****************************************************************************/

#include <AsyncAudioCodecAmbe.h>
#include <AsyncAudioDecimator.h>



/****************************************************************************
 *
 * Namespaces to use
 *
 ****************************************************************************/


using namespace Async;
using namespace std;


/****************************************************************************
 *
 * Defines & typedefs
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Local class definitions
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Prototypes
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Exported Global Variables
 *
 ****************************************************************************/




/****************************************************************************
 *
 * Local Global Variables
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Public member functions
 *
 ****************************************************************************/


namespace {
    /*
        Multiton pattern template. It's similar to the singleton pattern, but
        enables multiple instances through the use of keys.
        NOTE: Manual destruction must be done before program exit. Not thread-safe.

        class Foo : public Multiton<Foo> {};
        Foo &foo = Foo::getRef("foobar");
        foo.bar();
        Foo::destroyAll();
     */

    template <typename T, typename Key = map<string,string> >
    class Multiton
    {
    public:
        static void destroyAll()
        {
            for (typename map<Key, T*>::const_iterator it = instances.begin(); it != instances.end(); ++it)
                delete (*it).second;
            instances.clear();
        }

        static void destroy(const Key &key)
        {
            typename map<Key, T*>::iterator it = instances.find(key);

            if (it != instances.end()) {
                delete (*it).second;
                instances.erase(it);
            }
        }

        static T* getPtr(const Key &key)
        {
            const typename map<Key, T*>::const_iterator it = instances.find(key);

            if (it != instances.end())
                return (T*)(it->second);

            T* instance = T::create(key);
            instances[key] = instance;
            return instance;
        }

        static T& getRef(const Key &key)
        {
            return *getPtr(key);
        }

    protected:
        Multiton() {}
        ~Multiton() {}

    private:
        Multiton(const Multiton&);
        Multiton& operator=(const Multiton&);

        static map<Key, T*> instances;
    };

    template <typename T, typename Key>
    map<Key, T*> Multiton<T, Key>::instances;

    /**
     * @brief Implement shared Dv3k code here
     * (e.g. initialiation and protocol)
     */
    class AudioCodecAmbeDv3k : public AudioCodecAmbe, public Multiton<AudioCodecAmbeDv3k,AudioCodecAmbe::Options> {
    public:
        template <typename T = char>
        struct Buffer {
            Buffer(T *data = (T*) NULL, size_t length = 0) : data(data), length(length) {}
            T *data;
            size_t length;
        };

        static AudioCodecAmbeDv3k *create(const Options &options);

        virtual void init()
        {
          // init the Dv3k stick (ThumbDV dongle)
          char DV3K_REQ_PRODID[] = {DV3K_START_BYTE, 0x00, 0x01, DV3K_TYPE_CONTROL, DV3K_CONTROL_PRODID};
          Buffer<> init_packet = Buffer<>(DV3K_REQ_PRODID,sizeof(DV3K_REQ_PRODID));
          m_state = RESET;
          send(init_packet);
           t_buffer = Buffer<>(new char[512], sizeof(size_t));
        } /* void */

        virtual void prodid()
        {
          // reads the product id from dv3k stick, just to give it out for debug purposes
          char DV3K_REQ_PRODID[] = {DV3K_START_BYTE, 0x00, 0x01,
                                   DV3K_TYPE_CONTROL, DV3K_CONTROL_PRODID};
          Buffer<> prodid_packet = Buffer<>(DV3K_REQ_PRODID,sizeof(DV3K_REQ_PRODID));
          send(prodid_packet);
          m_state = PRODID;  /* state is requesting prod-id of Stick */
        } /* getProdId */

        virtual void versid()
        {
          // reads the version id from dv3k stick, just to give it out for debug purposes
          char DV3K_REQ_VERSID[] = {DV3K_START_BYTE, 0x00, 0x01,
                                   DV3K_TYPE_CONTROL, DV3K_CONTROL_VERSTRING};
          Buffer<> versid_packet = Buffer<>(DV3K_REQ_VERSID,sizeof(DV3K_REQ_VERSID));
          send(versid_packet);
          m_state = VERSID;  /* state is requesting version-id of Stick */
        } /* versid */

        virtual void ratep()
        {
          // set the rate and some other params (bausrate, codec, ....) to the Dv3k stick.
          char DV3K_REQ_RATEP[] = {DV3K_START_BYTE, 0x00, 0x07,
                                   DV3K_TYPE_CONTROL, 0x40, 0x0b, 0x03,
                                   0x09, 0x21, 0x32, 0x00};
          Buffer<> ratep_packet = Buffer<>(DV3K_REQ_RATEP,sizeof(DV3K_REQ_RATEP));
          send(ratep_packet);
          m_state = RATEP;  /* state is requesting version-id of Stick */
        } /* ratep */


        /* method to prepare incoming frames from network to be decoded later */
        virtual Buffer<> packForDecoding(const Buffer<> &buffer) { return buffer; }


        /* method to handle prepared encoded Data from network and them to AudioSink */
        virtual void unpackDecoded(const Buffer<> &buffer, Buffer<float> &out)
        {
          unsigned char *s8_samples = reinterpret_cast<unsigned char *>(buffer.data);
          size_t b = 0;
          float t[buffer.length];

          // starts with 4 to omit the DV3K-HEADER
          for (int a=DV3K_AMBE_HEADER_IN_LEN; a < (int)buffer.length-2; a+=2)
          {
            int16_t w = (s8_samples[a] << 8) | (s8_samples[a+1] << 0);
            // 16384.0 has been calculated experimental, is a subject to change!
            // maybe a simple audio compressor/clipper could be implemented here
            // by analyzing the decoded audio level
            t[b++] = (float) (w / 16384.0);
          }
          out.data = t;
          out.length = b-1;
        } /* unpackDecoded */

        /* method to prepare incoming Audio frames from local RX to be encoded later */
        virtual Buffer<> packForEncoding(const Buffer<> &buffer)
        {
          const unsigned char DV3K_VOICE_FRAME[] =
             {DV3K_START_BYTE, 0x01, 0x42, DV3K_TYPE_AUDIO, 0x00, 0xa0};
          int DV3K_VOICE_FRAME_LEN = 6;

          if (DV3K_AUDIO_LEN != buffer.length)
          {
            cerr << "*** ERROR: Buffer invalid!" << endl;
          }

          Buffer<> dv3k_buffer =
               Buffer<>(new char[330], sizeof(size_t));

          memcpy(dv3k_buffer.data, DV3K_VOICE_FRAME, sizeof(DV3K_VOICE_FRAME));
          memcpy(dv3k_buffer.data + DV3K_VOICE_FRAME_LEN, buffer.data, buffer.length);
          dv3k_buffer.length = DV3K_VOICE_FRAME_LEN + DV3K_AUDIO_LEN;
          return dv3k_buffer;
        } /* packForEncoding */

        virtual Buffer<> unpackEncoded(const Buffer<> &buffer)
        {
          return buffer;
        } /* unpackEncoded */

        /**
         * @brief 	encoded Ambe stream from NetRx -> 
         * @param 	buf  Buffer containing encoded samples
         * @param 	size The size of the buffer
         */
        virtual void writeEncodedSamples(void *buf, int size)
        {
          // const char DV3K_AMBE_HEADERFRAME[] = 
          // {DV3K_START_BYTE, 0x00, 0x0b, DV3K_TYPE_AMBE, 0x01, 0x48};
          const int DV3K_AMBE_HEADERFRAME_LEN = 6;
          
          const unsigned char DV3K_AMBE_HEADERFRAMEOUT[] = {DV3K_START_BYTE, 0x00, 0x0e,
                                                          DV3K_TYPE_AMBE, 0x40, 0x01, 0x48};
          const int DV3K_AMBE_HEADERFRAMEOUT_LEN = 7;
          
          const unsigned char DV3K_WAIT[] = {0x03, 0xa0};
          const int DV3K_WAIT_LEN = 2;
          
          cout << "writeEncodedSamples from NetRx " << size << endl;
          Buffer<> buffer;
          buffer.data = reinterpret_cast<char*>(buf);
          buffer.length = size;

          char ambe_to_dv3k[DV3K_AMBE_HEADERFRAMEOUT_LEN + DV3K_AMBE_FRAME_LEN + DV3K_WAIT_LEN];
          memcpy(ambe_to_dv3k, DV3K_AMBE_HEADERFRAMEOUT, DV3K_AMBE_HEADERFRAMEOUT_LEN); 
          memcpy(ambe_to_dv3k + DV3K_AMBE_HEADERFRAMEOUT_LEN, buffer.data + DV3K_AMBE_HEADERFRAME_LEN, DV3K_AMBE_FRAME_LEN);
          memcpy(ambe_to_dv3k + DV3K_AMBE_HEADERFRAMEOUT_LEN + DV3K_AMBE_FRAME_LEN, DV3K_WAIT, DV3K_WAIT_LEN);
          
          Buffer<>ambe_frame;
          ambe_frame = Buffer<>(ambe_to_dv3k, DV3K_AMBE_HEADERFRAMEOUT_LEN + DV3K_AMBE_FRAME_LEN + DV3K_WAIT_LEN);
          send(ambe_frame);
        } /* writeEncodedSamples */

        virtual void send(const Buffer<> &packet) = 0;

        virtual void callback(const Buffer<> &buffer)
        {
          size_t tlen = 0;
          
          // create a new buffer to handle the received data
          // t_buffer = Buffer<>(new char[512], sizeof(size_t));
          memcpy(t_buffer.data + stored_bufferlen, buffer.data, buffer.length);
          t_buffer.length = buffer.length + stored_bufferlen;
          stored_bufferlen = t_buffer.length;
          
          while(t_buffer.length >= DV3K_HEADER_LEN)
          {
            for (size_t a=0; a < t_buffer.length; a++)  
            {                // seek for 0x61
              if (t_buffer.data[a] == DV3K_START_BYTE && t_buffer.length-a > DV3K_HEADER_LEN)
              {
                tlen = t_buffer.data[a+1]*256 + t_buffer.data[a+2]; // get length of inbuff
                if (tlen + DV3K_HEADER_LEN <= t_buffer.length)
                {
                  Buffer<> dv3k_buffer = Buffer<>(new char[t_buffer.length+1], sizeof(size_t));
                  memcpy(dv3k_buffer.data, t_buffer.data + a, DV3K_HEADER_LEN + tlen);
                  dv3k_buffer.length = DV3K_HEADER_LEN + tlen;
                  handleBuffer(dv3k_buffer);
                
                  size_t pos = a + DV3K_HEADER_LEN + tlen;
                  memmove(t_buffer.data, t_buffer.data + pos, t_buffer.length - pos);
                  t_buffer.length -= pos;
                  stored_bufferlen = t_buffer.length;
                  break;
                }
                else
                {
                  return;
                }
              }
            }
          }
        } /* callback */


        // a typical Dv3K-Header will start with 0x61, the length (2 bytes) and the
        // type (0x00, 0x00, 0x02) will follow.
        // if not it maybe following data from the serial line/AMBEServer to feed the
        // ring buffer...
        virtual void handleBuffer(const Buffer <> inbuffer)
        {
          // we have 3 types:
          // 0x00 - Command byte
          // 0x01 - AMBE encoded stream
          // 0x02 - Audiostream
          int type = inbuffer.data[3];
                  
          /* test the type of incoming frame */
          if (type == DV3K_TYPE_CONTROL)
          {
            if (m_state == RESET) 
            {
              /* reset the device just to be sure */
              cout << "--- DV3K: Reset OK" << endl;
              prodid();
            }
            else if (m_state == PRODID)
            {
              /* give out product name of DV3k */
              cout << "--- DV3K (ProdID): "  << inbuffer.data+5 << endl;
              versid();            
            }
            else if (m_state == VERSID)
            {
              /* give out version of DV3k */
              cout << "--- DV3K (VersID): " << inbuffer.data+5 << endl;
              ratep();                
            }
            else if (m_state == RATEP)
            {
              /* sending configuration/rate params to DV3k was OK*/
              cout << "--- DV3K: Ready" << endl;
              m_state = READY;
            }
          }
          /* test if buffer contains encoded frame (AMBE) to the network */
          else if (type == DV3K_TYPE_AMBE)
          {
            AudioEncoder::writeEncodedSamples(inbuffer.data, inbuffer.length);
          }
          /* or is a raw 8kHz audio frame from Dv3K device*/
          else if (type == DV3K_TYPE_AUDIO)
          {
            // unpack decoded frame
            Buffer<float> unpacked;
            unpackDecoded(inbuffer, unpacked);

            // pass decoded samples into sink
            if (unpacked.length > 0)
            {
              AudioDecoder::sinkWriteSamples(unpacked.data, unpacked.length);
            }
          }
          else
          {
            /* frame is unknown */
            cout << "--- WARNING: received unkown DV3K type." << endl;
          }
        } /* handleBuffer */
        

        /* method is called up to send encoded AMBE frames to BM network */
        virtual int writeSamples(const float *samples, int count)
        {
           // store incoming floats in a buffer until 160 or more samples
           // are received
          Buffer<> ambe_buf;

          memcpy(inbuf + bufcnt, samples, sizeof(float)*count);
          bufcnt += count;
          char t_data[bufcnt];
          while (bufcnt >= DV3K_AUDIO_LEN)
          {
            for (int a = 0; a<DV3K_AUDIO_LEN; a++)
            {
              // This is a HACK!
              // the INTERNAL_SAMPLE_RATE is normaly 16000 but the DV3k stick
              // expects 8000. In the Logic class an Encoder instance must be
              // created that the audio stream from linked logics could be
              // received
              // its working for now, but it can not be the goal
              int32_t w = (int) ((inbuf[a] + inbuf[a+1]) * 32768.0);
              // printf("%d ", w);
              t_data[a] = (w & 0xff00) >> 8;
              t_data[a+1] = (w & 0x00ff) >> 0;
            }
            //cout << endl;

            ambe_buf.data = t_data;
            ambe_buf.length = DV3K_AUDIO_LEN;
            Buffer<> packet = packForEncoding(ambe_buf);

            // sends the ambe stream to the brandmeister network
            send(packet);

            bufcnt -= DV3K_AUDIO_LEN;
            memmove(inbuf, inbuf + DV3K_AUDIO_LEN-1, bufcnt);
          }
          inbuf[bufcnt] = '\0';
          return count;
        } /* writeSamples */

    protected:
      static const char DV3K_TYPE_CONTROL = 0x00;
      static const char DV3K_TYPE_AMBE = 0x01;
      static const char DV3K_TYPE_AUDIO = 0x02;
      static const char DV3K_HEADER_LEN = 0x04;
      static const char DSTAR_AUDIO_BLOCK_SIZE=160;

      static const char DV3K_START_BYTE = 0x61;

      static const char DV3K_CONTROL_RATEP  = 0x0A;
      static const char DV3K_CONTROL_PRODID = 0x30;
      static const char DV3K_CONTROL_VERSTRING = 0x31;
      static const char DV3K_CONTROL_RESET = 0x33;
      static const char DV3K_CONTROL_READY = 0x39;
      static const char DV3K_CONTROL_CHANFMT = 0x15;

      static const uint16_t DV3K_AUDIO_LEN = 320;
      static const uint16_t DV3K_AMBE_HEADER_IN_LEN = 6;
      static const uint16_t DV3K_AMBE_HEADER_OUT_LEN = 7;
      static const uint16_t DV3K_AMBE_FRAME_LEN = 9;
      static const uint16_t REWIND_DMR_AUDIO_FRAME_LENGTH = 27;

      /**
      * @brief 	Default constuctor
      */
      //AudioCodecAmbeDv3k(void) : device_initialized(false) {}
      AudioCodecAmbeDv3k(void) : m_state(OFFLINE), bufcnt(0),
      r_bufcnt(0), rxavg(16384.0) {}

    private:
      enum STATE {
        OFFLINE, RESET, INIT, PRODID, VERSID, RATEP, READY, WARNING, ERROR
      };
      STATE m_state;
      uint32_t stored_bufferlen;
      int act_framelen;
      Buffer<> t_buffer;
      int t_b;
      float inbuf[640];
      uint32_t bufcnt;
      uint32_t r_bufcnt;
      char r_buf[100];
      float rxavg;

      AudioCodecAmbeDv3k(const AudioCodecAmbeDv3k&);
      AudioCodecAmbeDv3k& operator=(const AudioCodecAmbeDv3k&);
    };

    /**
     * TODO: Implement communication with Dv3k via UDP here.
     */
    class AudioCodecAmbeDv3kAmbeServer : public AudioCodecAmbeDv3k {
    public:
      /**
      * @brief  Default constuctor
      *         TODO: parse options for IP and PORT
      */
      AudioCodecAmbeDv3kAmbeServer(const Options &options)
      {
        Options::const_iterator it;

        if ((it=options.find("AMBESERVER_HOST"))!=options.end())
        {
          ambehost = (*it).second;
        }
        else
        {
          cout << "*** ERROR: Parameter AMBE_(ENC|DEC)_AMBESERVER_HOST not defined." << endl;
          throw;
        }

        if((it=options.find("AMBESERVER_PORT"))!=options.end())
        {
          ambeport = atoi((*it).second.c_str());
        }
        else
        {
          cout << "*** ERROR: Parameter AMBE_(ENC|DEC)_AMBESERVER_PORT not defined." << endl;
          throw;
        }
        udpInit();
      } /* AudioCodecAmbeDv3kAmbeServer */

      /* initialize the udp socket */
      void udpInit(void)
      {
        if (ip_addr.isEmpty())
        {
          dns = new DnsLookup(ambehost);
          dns->resultsReady.connect(mem_fun(*this,
                &AudioCodecAmbeDv3kAmbeServer::dnsResultsReady));
          return;
        }

        delete ambesock;
        ambesock = new UdpSocket();
        ambesock->dataReceived.connect(mem_fun(*this,
                         &AudioCodecAmbeDv3kAmbeServer::callbackUdp));

        cout << "--- DV3k: UdpSocket " << ambehost << ":" << ambeport
             << " created." << endl;
        init();
      } /* udpInit */

      /* called-up when dns has been resolved */
      void dnsResultsReady(DnsLookup& dns_lookup)
      {
        vector<IpAddress> result = dns->addresses();

        delete dns;
        dns = 0;
        if (result.empty() || result[0].isEmpty())
        {
          ip_addr.clear();
          cout << "*** ERROR: Could not found host." << endl;
          return;
        }
        ip_addr = result[0];
        udpInit();
      } /* dnsResultReady */

      virtual void send(const Buffer<> &packet)
      {
        ambesock->write(ambehost, ambeport, packet.data, packet.length);
      } /* send */

      ~AudioCodecAmbeDv3kAmbeServer()
      {
        delete dns;
        dns = 0;
        delete ambesock;
      } /* ~AudioCodecAmbeDv3kAmbeServer */

   protected:
      virtual void callbackUdp(const IpAddress& addr, uint16_t port,
                                         void *buf, int count)
      {
        callback(Buffer<>(reinterpret_cast<char *>(buf), count));
      } /* callbackUdp */

    private:

      int ambeport;
      string ambehost;
      UdpSocket * ambesock;
      IpAddress	ip_addr;
      DnsLookup	*dns;

      AudioCodecAmbeDv3kAmbeServer(const AudioCodecAmbeDv3kAmbeServer&);
      AudioCodecAmbeDv3kAmbeServer& operator=(const AudioCodecAmbeDv3kAmbeServer&);
    };

    /**
     * TODO: Implement communication with Dv3k via TTY here.
     */
    class AudioCodecAmbeDv3kTty : public AudioCodecAmbeDv3k {
    public:
      /**
      * @brief 	Default constuctor
      */
      AudioCodecAmbeDv3kTty(const Options &options) {
        Options::const_iterator it;
        int baudrate;
        string device;

        if ((it=options.find("TTY_DEVICE"))!=options.end())
        {
          device = (*it).second;
        }
        else
        {
          cout << "*** ERROR: Parameter AMBE_(ENC|DEC)_TTY_DEVICE not defined." << endl;
          return;
        }

        if((it=options.find("TTY_SPEED"))!=options.end())
        {
          baudrate = atoi((*it).second.c_str());
        }
        else
        {
          cout << "*** ERROR: Parameter AMBE_(ENC|DEC)_TTY_SPEED not defined." << endl;
          return;
        }

        if (baudrate != 230400 && baudrate != 460800)
        {
          cout << "*** ERROR: AMBE_(ENC|DEC)_TTY_BAUDRATE must be 230400 or 460800." << endl;
          return;
        }

        serial = new Serial(device);
        serial->setParams(baudrate, Serial::PARITY_NONE, 8, 1, Serial::FLOW_NONE);
        if (!(serial->open(true)))
        {
          cerr << "*** ERROR: Can not open device " << device << endl;
          return;
        }
        serial->charactersReceived.connect(
             sigc::mem_fun(*this, &AudioCodecAmbeDv3kTty::callbackTty));
        init();
      } /* AudioCodecAmbeDv3kTty */

      virtual void send(const Buffer<> &packet) 
      {
        //cout << "sending to tty device " << packet.length << endl;
        serial->write(packet.data, packet.length);
      } /* send */

      ~AudioCodecAmbeDv3kTty()
      {
        serial->close();
        delete serial;
      } /* ~AudioCodecAmbeDv3kTty */

    protected:
      virtual void callbackTty(const char *buf, int count)
      {
        callback(Buffer<>(const_cast<char *>(buf),count));
      }

    private:
      Serial *serial;

      AudioCodecAmbeDv3kTty(const AudioCodecAmbeDv3kTty&);
      AudioCodecAmbeDv3kTty& operator=(const AudioCodecAmbeDv3kTty&);
    };

    AudioCodecAmbeDv3k *AudioCodecAmbeDv3k::create(const Options &options) {
      Options::const_iterator type_it = options.find("TYPE");
      if(type_it!=options.end())
      {
        if(type_it->second=="AMBESERVER")
        {
          return new AudioCodecAmbeDv3kAmbeServer(options);  
        }          
        else if(type_it->second=="TTY")
        {
          return new AudioCodecAmbeDv3kTty(options);  
        }
        else
        {
          cout << "unknown Ambe codec TYPE" << endl;
          throw;
        }
      }
      else
      {
        cout << "unspecified Ambe codec TYPE" << endl;
        throw;
      }
    } /* AudioCodecAmbeDv3k::create */
}


AudioCodecAmbe *AudioCodecAmbe::create(const Options &options) 
{
  
  Options::const_iterator type_it;
  Options t_options;
  for (type_it=options.begin(); type_it!=options.end(); type_it++)
  {
    std::string t_type = type_it->first;
    if (t_type.find("AMBE_ENC_")!=std::string::npos
        ||t_type.find("AMBE_DEC_")!=std::string::npos)
    {
      t_type.erase(0,9);
    }
    t_options[t_type] = type_it->second;
  }
  
  type_it=t_options.find("TYPE");
  if(type_it!=options.end())
  {
    if (type_it->second=="AMBESERVER" || type_it->second=="TTY")
    {
      return AudioCodecAmbeDv3k::getPtr(t_options);  
    }
    else
      throw "unknown Ambe codec TYPE";
  }
  else
  {
    throw "unspecified Ambe codec TYPE";  
  }
} /* AudioCodecAmbe::create */

/*
 * This file has not been truncated
 */