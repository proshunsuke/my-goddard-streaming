/*
 * File:   goddard.h
 * Author: Jae Chung (goos@cs.wpi.edu)
 * Date:   Jan 2005
 *
 * Goddard server implementation
 */

#ifndef ns_goddard_h
#define ns_goddard_h

#include "mchannel_proc.h"
#include "frame.h"
 
struct goddard_params {
    int    maxscale;            // The number of scale levels
    int    frmsize[MAX_SCALE];  // Media frame size
    double interval[MAX_SCALE]; // Media frame transmission interval
    int    random;              // If 1 add randomness to the interval
    int    fragsize;            // Media frame fragment size (for UDP)
};

struct goddard_vars {
    int state;         // Server state: STOPPED, BUFFERING, STREAMING
    int scale;         // Selected media scale level
    int exp_cseq;      // Expected control msg (from client) sequence number
    int last_mseq;     // Last media frame sequence number
    int last_cseq;     // Last control msg sequence number
    int last_pseq;     // Last packet-pair frame sequence number
    double buf_factor; // buffering rate factor ( brate = factor * stream rate)
};

class Goddard;

// Schedules next media frame transmission time
class FrameSendTimer : public TimerHandler {
 public:
      FrameSendTimer(Goddard* t) : TimerHandler(), t_(t) {}
      inline virtual void expire(Event*);
 protected:
      Goddard* t_;
};


class Goddard : public MchannelProcess {
 public:
      Goddard();
      void send_frame();  // called by SendTimer:expire
      void process_data(int size, AppData* data); // Process incoming ctr msg

 protected:
      void process_ctrmsg(struct goddard_frame *frm);
      void send_ctrmsg(const char *msg);
      void send_pktpair();
      int command(int argc, const char*const* argv);
      void start();       // Start sending media frames
      void stop();        // Stop sending data packets (Sender)

 private:
      void init();
      double next_send_time();

      struct goddard_params gddp_;
      struct goddard_vars   gddv_;

      FrameSendTimer send_timer_;  // FrameSendTimer
};

#endif

