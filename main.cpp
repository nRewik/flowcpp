#include <iostream>
#include <vector>

#include <flowcpp/flow.h>

#define RESELECT_DEBUG 1

enum class counter_action_type {
  thunk,
  increment,
  decrement,
};

struct increment_action {
  flow::any payload() const { return _payload; }
  flow::any type() const { return _type; }
  flow::any meta() const { return _meta; }
  bool error() const { return _error; }

  int _payload = {1};
  counter_action_type _type = {counter_action_type::increment};
  flow::any _meta;
  bool _error = false;
};

struct decrement_action {
  flow::any payload() const { return _payload; }
  flow::any type() const { return _type; }
  flow::any meta() const { return _meta; }
  bool error() const { return _error; }

  int _payload = {1};
  counter_action_type _type = {counter_action_type::decrement};
  flow::any _meta;
  bool _error = false;
};

struct counter_state {
  std::string to_string() { return "counter: " + std::to_string(_counter); }

  int _counter{0};
};

auto reducer = [](counter_state state, flow::action action) {
  int multiplier = 1;
  auto type = action.type().as<counter_action_type>();
  switch (type) {
    case counter_action_type::decrement:
      multiplier = -1;
      break;
    case counter_action_type::increment:
      multiplier = 1;
      break;
    default:
      break;
  }

  auto payload = action.payload().as<int>();
  state._counter += multiplier * payload;
  return state;
};

std::string to_string(counter_action_type type) {
  switch (type) {
    case counter_action_type::increment:
      return "inc";
    case counter_action_type::decrement:
      return "dec";
    case counter_action_type::thunk:
      return "thunk";
  }
}

auto logging_middleware = [](flow::basic_middleware<counter_state>) {
  return [=](const flow::dispatch_t &next) {
    return [=](flow::action action) {
      auto next_action = next(action);
      std::cout << "after dispatch: " << to_string(action.type().as<counter_action_type>()) << std::endl;
      return next_action;
    };
  };
};


//
using args_t = std::vector<flow::any>;
using func_t = std::function<flow::any(args_t)>;
using selector_t = std::function<flow::any(flow::any)>;
using selector_creator_t = std::function<selector_t(std::vector<selector_t>, func_t)>;
using memoize_func_t = std::function<flow::any(args_t)>;


//
using equality_check_t = std::function<bool(flow::any, flow::any)>;
auto default_memoize = [](std::vector<equality_check_t> equality_checks){
  return [=](func_t func){
    auto last_args = std::vector<flow::any>();
    auto last_result = flow::any(); 

    return [=](args_t args) mutable -> flow::any{
      if ( last_args.size() == args.size() ){
        if (args.size() != equality_checks.size()){
          throw std::runtime_error("Default Memoize Error: size of args and equility_checks are mismatch.");
        }

        auto all_args_are_equal = true;
        for(int i = 0; i < args.size(); i++){
          auto arg_equal = equality_checks[i]( args[i], last_args[i] );
          if (!arg_equal){ 
            all_args_are_equal = false; 
            break; 
          }
        }

        if (all_args_are_equal){ 
          std::cout << "use cache" << "\n";
          return last_result; 
        }
      }

      auto new_result = func(args);
      last_args = args;
      last_result = new_result;
      return last_result;
    };
  };
};

using map_key_t = std::function<std::string(flow::any)>;
auto map_memoize = [](std::vector<map_key_t> keys){
  return [=](func_t func){
    auto result_map = std::unordered_map<std::string, flow::any>{};
    return [=](args_t args) mutable -> flow::any{

      if (args.size() != keys.size()){
          throw std::runtime_error("Map Memoize Error: size of args and keys are mismatch.");
      }

      auto key = std::string();
      for(int i = 0; i < args.size(); i++){
        key = key + keys[i](args[i]);
      }

      if (result_map.find(key) != result_map.end()){
#ifdef RESELECT_DEBUG
        std::cout << "use cache" << "\n";        
#endif
        return result_map[key];
      }

      auto new_result = func(args);
      result_map[key] = new_result;
      return new_result;
    };
  };
};

auto create_selector_creator(memoize_func_t memoized_result_func) -> selector_creator_t{
  return [=](auto selectors, auto func){
    return [=](auto state){
        std::vector<flow::any> params;
        for (const auto selector: selectors) {
          auto param = selector(state);
          params.push_back(param);
        }      
        return memoized_result_func(params);
    };
  };
};

auto create_selector(std::vector<selector_t> selectors, func_t func, std::function<memoize_func_t(func_t)> memoize) -> selector_t{

#ifdef RESELECT_DEBUG
  auto memoized_result_func = memoize(
    [func = func](args_t args){
      std::cout << "recompute" << std::endl;
      return func(args);
    });
#else
  auto memoized_result_func = memoize(func);
#endif
  return create_selector_creator(memoized_result_func)(selectors, func);
}


void reselect_example() {
  std::cout << "Start: Selector example" << std::endl;

  struct SubState{
    int sub_id;
  };

  struct RootState{
    int id;
    SubState sub_state;
  };

  auto id_selector = selector_t{
    [](flow::any state){
      return state.as<RootState>().id;
    }};

  auto sub_id_selector = selector_t{
    [](flow::any state){
      return state.as<RootState>().sub_state.sub_id;
    }};

  auto func = func_t{
    [](std::vector<flow::any> params){
      auto id = params.at(0).as<int>();
      auto sub_id = params.at(1).as<int>();
      return id * sub_id;
    }};

  auto root_state = RootState();
  root_state.id = 2;
  root_state.sub_state.sub_id = 4;

  /* default memoize
  auto int_equals = equality_check_t{ [](flow::any left, flow::any right){
    return left.as<int>() == right.as<int>();
  }};
  auto equality_checks = std::vector<equality_check_t>{int_equals, int_equals};
  auto multiply_selector = create_selector({id_selector,sub_id_selector}, func, default_memoize(equality_checks));
  */

  ///* map memoize
  auto int_key = map_key_t{ [](flow::any x){
    return std::to_string(x.as<int>());
  }};
  auto keys = std::vector<map_key_t>{int_key, int_key};
  auto multiply_selector = create_selector({id_selector,sub_id_selector}, func, map_memoize(keys));
  //*/

  auto result_1 = multiply_selector(root_state);
  std::cout << result_1.as<int>() << std::endl;

  root_state.sub_state.sub_id = 5;
  auto result_2 = multiply_selector(root_state);
  std::cout << result_2.as<int>() << std::endl;

  root_state.sub_state.sub_id = 5;
  auto result_3 = multiply_selector(root_state);
  std::cout << result_3.as<int>() << std::endl;

  root_state.id = 10;
  auto result_4 = multiply_selector(root_state);
  std::cout << result_4.as<int>() << std::endl;

  auto result_5 = multiply_selector(root_state);
  std::cout << result_5.as<int>() << std::endl;

  root_state.id = 2;
  root_state.sub_state.sub_id = 4;
  auto result_6 = multiply_selector(root_state);
  std::cout << result_6.as<int>() << std::endl;
}


void simple_example() {
  std::cout << "Start: Simple example" << std::endl;

  auto store = flow::create_store_with_action<counter_state>(reducer, counter_state{}, increment_action{5});
  auto disposable = store.subscribe([](counter_state state) { std::cout << state.to_string() << std::endl; });

  store.dispatch(increment_action{2});
  store.dispatch(decrement_action{10});
  disposable.dispose();  // call dispose to stop notification prematurely
  store.dispatch(increment_action{3});
  store.dispatch(decrement_action{6});

  std::cout << "End: Simple example " << store.state().to_string() << std::endl;
}

void thunk_middleware_example() {
  std::cout << "Start: Thunk Middleware example" << std::endl;

  auto store = flow::apply_middleware<counter_state>(
      reducer, counter_state(), {flow::thunk_middleware<counter_state, counter_action_type>, logging_middleware});

  std::cout << store.state().to_string() << std::endl;

  store.dispatch(flow::thunk_action<counter_state, counter_action_type>{[&](auto dispatch, auto get_state) {
    dispatch(increment_action{1});
    dispatch(decrement_action{2});
    dispatch(increment_action{3});
  }});

  store.dispatch(flow::thunk_action<counter_state, counter_action_type>{[&](auto dispatch, auto get_state) {
    dispatch(increment_action{4});
    dispatch(decrement_action{5});
    dispatch(increment_action{6});
  }});

  std::cout << "End: Thunk Middleware example " << store.state().to_string() << std::endl;
}

int main() {
  simple_example();
  std::cout << "------------------------------" << std::endl;
  thunk_middleware_example();
  std::cout << "------------------------------" << std::endl;
  reselect_example();
  return 0;
}
