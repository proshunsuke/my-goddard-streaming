/*
 * File:   gplayer.h
 * Author: Jae Chung (goos@cs.wpi.edu)
 * Date:   Jan 2005
 *
 * Goddard client (Gplayer) implementation
 */

#ifndef ns_gplayer_h
#define ns_gplayer_h

#include "mchannel_proc.h"
#include "frame.h"
#include "gframe_queue.h"
 
struct gplayer_params {
    double pktpair_timeout;     // Packet pair timeout interval
    double buf_factor;          // Buffering factor (rate = factor*stream_rate)
    double play_buf_len;        // Playout buffer threshold (in seconds)
    double loss_mon_interval;   // Loss monitoring interval
    double upscale_interval;    // Media scale up decision interval
    double upscale_lt_factor;   // Upscale limit time factor
    double loss_rate_min_thrsh; // Upscale decision boundary frame loss rate
    double loss_rate_max_thrsh; // Downscale decision boundary frame loss rate

};

struct gplayer_vars {
    int state;                // Gplayer state: STOPPED, BUFFERING, STREAMING
    int eos;                  // End-of-streaming flag (set when recv STOP msg)
    double bitrate[MAX_SCALE];// Place to store negoticated levels
    int fps[MAX_SCALE];       // Place to store frame/sec info
    int maxscale;             // Maximum scale for the current stream
    int scale;                // Currently selected (reqested) scale level
    int exp_mseq;             // Expected media frame sequence number
    int exp_cseq;             // Expected control msg sequence number
    int exp_pseq;             // Expected pktpair frame sequence number
    int last_cseq;            // Last control msg (to server) sequence number)
    double t_pktpair_1st;     // Time receved the first packet of a pair
    int congestion;           // Congestion flag
};

struct gplayer_stats {
    /* stream life-long stats */
    double est_cap_max;  // Maximum of the estimated capacities
    double est_cap_min;  // Minimum of the estimated capacities

    /* rebuffing related stats */
    double t_play;       // Start of continous play or rebuf_cnt adjustment
    int rebuf_cnt;       // Rebuffer counter (currently not used)
    int rebuf_scale;     // Scale level at the last rebuffer event

    /* short-term stats */
    double t_last;       // Start of short-term statistics collection
    double t_last_frm;   // Time of last frame reception or reset after scaling
    double est_cap;      // Estimated bottleneck capacity
    int rcvd_bytes;      // # of receved bytes since the last buffering period
    int rcvd_frms;       // # of receved frames since the last buffering period
    int lost_frms;       // # of lost frames since last the buffering period
};

struct frame_assembly_info {
    int seq;        // The sequence number of the current frame working on
    int exp_nsegs;  // Expected number of segements for this frame
    int rcv_nsegs;  // Received number of segements for this frame
    int rcv_bytes;  // Received bytes for this frame
};

class Gplayer;

// Schedules next static computation for media up-scaling decision
class UpscaleTimer : public TimerHandler {
 public:
      UpscaleTimer(Gplayer* t) : TimerHandler(), t_(t) {}
      inline virtual void expire(Event*);
 protected:
      Gplayer* t_;
};

// Schedules next packet pair request in case of timeout due to packet loss
class PktpairTimer : public TimerHandler {
 public:
      PktpairTimer(Gplayer* t) : TimerHandler(), t_(t) {}
      inline virtual void expire(Event*);
 protected:
      Gplayer* t_;
};

// Schedules next frame display
class DisplayTimer : public TimerHandler {
 public:
      DisplayTimer(Gplayer* t) : TimerHandler(), t_(t) {}
      inline virtual void expire(Event*);
 protected:
      Gplayer* t_;
};


class Gplayer : public MchannelProcess {
 public:
      Gplayer();
      void process_upscale();          // Called by UpscaleTimer::expire
      void pktpair_timeout_handler();  // Called by PktpairTimer::expire
      void play_video();               // Called by Displaytimer::expire
      void process_data(int size, AppData* data);  // Process incoming data

 protected:
      void process_ctrmsg(struct goddard_frame *frm);
      void process_medframe(struct goddard_frame *frm);
      void process_pktpair(struct goddard_frame *frm);
      void send_ctrmsg(const char *msg);
      int adjust_scale_level(int gentle);
      int command(int argc, const char*const* argv);

 private:
      void init();

      struct gplayer_params gpp_;
      struct gplayer_vars   gpv_;
      struct gplayer_stats  gps_;
      struct frame_assembly_info fasm_;

      Tcl_Channel tchan_;           // Place to write trace records
      char tmsg_[256];              // Trace message
      void gtrace(void);            // Print trace message

      GplayerFrameQueue play_q_;    // Playout buffer

      UpscaleTimer upscale_timer_;  // UpscaleTimer
      PktpairTimer pktpair_timer_;  // PktpairTimer
      DisplayTimer display_timer_;  // DisplayTimer
};

#endif
