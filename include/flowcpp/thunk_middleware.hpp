#pragma once

namespace flow {

template <class State>
using Thunk = std::function<void(const flow::dispatch_t&, const std::function<State()>)>;

template<class State,class ActionType>
auto thunk_middleware = [](flow::basic_middleware<State> middleware) {
  return [=](const flow::dispatch_t& dispatch) {
    return [=](flow::action action) -> flow::action {
      auto type = *static_cast<const ActionType*>(action.type());
      if (type == ActionType::thunk) {
        auto payload = *static_cast<const Thunk<State>*>(action.payload());
        payload(dispatch, middleware.get_state());
      }
      return dispatch(action);
    };
  };
};

} // namespace flow

// Example usage
/*
  struct ThunkAction {
    const void* payload() const { return &_payload; }
    const void* type() const { return &_type; }
    const void* meta() const { return _meta; }
    bool error() const { return _error; }

    flow::Thunk<AppState> _payload;
    AppActionType _type{AppActionType::thunk};
    void* _meta = nullptr;
    bool _error = false;
  };

  ThunkAction fetch_message(){
    return ThunkAction{ flow::Thunk<AppState>{ [&](auto dispatch, auto get_state){
      dispatch(StartFetchingAction{});
      // Async operation
      fetch$.subscribe(
        [=](const std::string& message){
          dispatch(ReceieveMessageAction{message});
          dispatch(FinishFetchingAction{});
        },
        [=](exception_ptr ex) {
          dispatch(ReceieveErrorAction{ex});
          dispatch(FinishFetchingAction{});
        });
    }}}
  }

  auto store = flow::apply_middleware<AppState>({flow::thunk_middleware<AppState,AppActionType>})(flow::create_store<AppState>)(reducer, AppState())
  store.dispatch(fetch_message());
*/
