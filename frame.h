/*
 * File:   frame.h
 * Author: Jae Chung (goos@cs.wpi.edu)
 * Date:   Jan 2005
 *
 * Defines media frame used by Goddard
 */

#define MAX_SCALE   7
#define MAX_MSGLEN 32

#define STOPPED   0
#define BUFFERING 1
#define STREAMING 2

#define CNTRL_CH  0
#define MEDIA_CH  1
#define PPAIR_CH  2

#define CNTRL_DATA 0
#define MEDIA_DATA 1
#define PPAIR_DATA 2

struct media_frame_info {
    int    seg_last;  // Last segment number (0 if not segmented)
    int    seg_cur;   // Current segment number
    double interval;  // Playout interval
};

struct control_frame_info {
    char   msg[MAX_MSGLEN]; // Control message
};

struct goddard_frame {
    int type;   // Frame type: CNTRL_DATA, PPAIR_DATA, MEDIA_DATA
    int size;   // Frame size
    int seq;    // Sequence number
    union {
	struct media_frame_info   med;
	struct control_frame_info ctr;
    } u;
};
