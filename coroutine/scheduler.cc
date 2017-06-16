
#include <functional>
#include "../base/all.hpp"
#include "scheduler.h"
#include "coroutine.h"
#include "event.h"

namespace rrr {

thread_local CoroScheduler coro_sched_{};
thread_local Coroutine* curr_coro_{nullptr};

Coroutine* Coroutine::CurrentCoroutine() {
  verify(curr_coro_ != nullptr);
  return curr_coro_;
}

void Coroutine::Create(const std::function<void()>& func) {
  auto sched = CoroScheduler::CurrentScheduler();
  auto coro = sched->GetCoroutine(func);
  // TODO
//  coro->Run(func);
  sched->new_coros_.push_back(coro);
  sched->Loop();
  sched->ReturnCoroutine(coro);
}

CoroScheduler* CoroScheduler::CurrentScheduler() {
  return &coro_sched_;
}

void CoroScheduler::AddReadyEvent(Event *ev) {
  ready_events_.push_back(ev);
}

Coroutine* CoroScheduler::GetCoroutine(const std::function<void()>& func) {
  Coroutine* c = new Coroutine(func);
  return c;
}

void CoroScheduler::ReturnCoroutine(Coroutine* coro) {
  delete coro;
}

void CoroScheduler::Loop(bool infinite) {

  do {
    while (ready_events_.size() > 0) {
      Event* e = ready_events_.front();
      ready_events_.pop_front();
      curr_coro_ = e->coro_;
      e->coro_->Continue();
    }
    while (new_coros_.size() > 0) {
      Coroutine* c = new_coros_.front();
      new_coros_.pop_front();
      curr_coro_ = c;
      c->Continue();
    }
  } while (infinite);
}

} // namespace rrr
