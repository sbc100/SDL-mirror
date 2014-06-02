#ifndef _SDL_nacl_eventqueue_h
#define _SDL_nacl_eventqueue_h

#include "SDL_mutex.h"

#include <queue>

class EventQueue {
public:
  EventQueue() {
    mu_ = SDL_CreateMutex();
  }

  ~EventQueue() {
    SDL_DestroyMutex(mu_);
  }

  SDL_Event* PopEvent() {
    SDL_LockMutex(mu_);
    SDL_Event* event = NULL;
    if (!queue_.empty()) {
      event = queue_.front();
      queue_.pop();
    }
    SDL_UnlockMutex(mu_);
    return event;
  }

   void PushEvent(SDL_Event* event) {
    SDL_LockMutex(mu_);
    queue_.push(event);
    SDL_UnlockMutex(mu_);
   } 

private:
  std::queue<SDL_Event*> queue_;
  SDL_mutex* mu_;
};

#endif // _SDL_nacl_eventqueue_h
