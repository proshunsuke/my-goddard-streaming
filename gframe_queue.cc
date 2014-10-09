/*
 * File:   gframe_queue.cc
 * Author: Jae Chung (goos@cs.wpi.edu)
 * Date:   Jan 2005
 *
 * Gplayer media playout queue implementation
 */

#include "gframe_queue.h"
#include <stdio.h>

GplayerFrameQueue::GplayerFrameQueue()
{
    qlen_ = 0;
    qlen_in_time_ = 0.0;
    head_ = NULL;
    tail_ = NULL;
}

void GplayerFrameQueue::enque(struct gplayer_frame *gfrm)
{
    qlen_++;
    qlen_in_time_ += gfrm->interval;

    if (head_ == NULL) {
	head_ = gfrm;
	tail_ = head_;
    }
    else {
	qlen_in_time_ += tail_->interval * (gfrm->seq - tail_->seq - 1);
	tail_->next = gfrm;
	tail_ = gfrm;
    }
}

struct gplayer_frame *GplayerFrameQueue::deque()
{
    struct gplayer_frame *tmp = head_;

    if (head_ != NULL) {
	head_ = head_->next;
	qlen_--;
	if (head_ == NULL) {
		qlen_in_time_ = 0.0;
	}
	else {
		qlen_in_time_ -= tmp->interval * (head_->seq - tmp->seq);
	}
    }

    return tmp;
}
