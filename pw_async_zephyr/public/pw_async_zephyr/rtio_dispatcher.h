// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#pragma once

#include <zephyr/rtio/rtio.h>

#include "pw_allocator_zephyr/sys_mem_block_allocator.h"
#include "pw_async/dispatcher.h"
#include "pw_async/task.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/timed_thread_notification.h"
#include "pw_thread/thread_core.h"

namespace pw::async {

class RtioDispatcher final : public Dispatcher, public thread::ThreadCore {
 public:
  explicit RtioDispatcher(struct rtio* r,
                          pw::allocator::zephyr::ZephyrAllocator& allocator)
      : allocator_(allocator), r_(r) {}

  // ThreadCore overrides:

  void Run() override PW_LOCKS_EXCLUDED(lock_);

  // Dispatcher overrides:

  void PostAt(Task& task, chrono::SystemClock::time_point time) override {
    lock_.lock();
    PostTaskInternal(task.native_type(), time);
    lock_.unlock();
  }
  bool Cancel(Task& task) override PW_LOCKS_EXCLUDED(lock_);

  // VirtualSystemClock overrides:
  chrono::SystemClock::time_point now() override {
    return chrono::SystemClock::now();
  }

 private:
  // Insert |task| into task_queue_ maintaining its min-heap property, keyed by
  // |time_due|.
  void PostTaskInternal(backend::NativeTask& task,
                        chrono::SystemClock::time_point time_due)
      PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    task.due_time_ = time_due;
    auto it_front = task_queue_.begin();
    auto it_behind = task_queue_.before_begin();
    while (it_front != task_queue_.end() && time_due > it_front->due_time_) {
      ++it_front;
      ++it_behind;
    }
    task_queue_.insert_after(it_behind, task);
    timed_notification_.release();
  }
  sync::InterruptSpinLock lock_;
  sync::TimedThreadNotification timed_notification_;
  // A priority queue of scheduled Tasks sorted by earliest due times first.
  IntrusiveList<backend::NativeTask> task_queue_ PW_GUARDED_BY(lock_);
  pw::allocator::zephyr::ZephyrAllocator& allocator_;
  struct rtio* const r_;
  bool stop_requested_;
};

}  // namespace pw::async
