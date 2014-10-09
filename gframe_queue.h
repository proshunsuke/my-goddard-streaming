/*
 * File:   gframe_queue.h
 * Author: Jae Chung (goos@cs.wpi.edu)
 * Date:   Jan 2005
 *
 * Gplayer media playout queue implementation
 */

#ifndef ns_gframe_queue_h
#define ns_gframe_queue_h

// Gplayer Frame Structure
struct gplayer_frame {
    int seq;                    // Sequence number
    int size;                   // Frame size
    double interval;            // Playout interval
    struct gplayer_frame *next;
};

// Queue for recieved frame playout
class GplayerFrameQueue {
 public:
    GplayerFrameQueue();
    void enque(struct gplayer_frame *gfrm);
    struct gplayer_frame *deque();
    int qlen() { return qlen_; }
    double qlen_in_time() { return qlen_in_time_; }

 private:
    int qlen_;
    double qlen_in_time_;
    struct gplayer_frame *head_;
    struct gplayer_frame *tail_;
};

#endif
