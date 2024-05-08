export module Yuubi.Event:Dispatcher;

import :Base;

// TODO: support multiple callbacks for single event
// Just use inheritance and have each callback accept
// base class and downcast internally
export class EventDispatcher {
    public:
        EventDispatcher(Event& event) : event(event) {}
        
        template<typename T, typename F>
        bool dispatch(const F& func) {
            if (this->event.getEventType() == T::getStaticType()) {
                this->event.handled |= func(static_cast<T&>(this->event));
                return true;
            }
            return false;
        }
    private:
        Event& event;
};
