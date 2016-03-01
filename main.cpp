#include <iostream>
#include <vector>

#include <flowcpp/flow.h>

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
using selector_t = std::function<flow::any(flow::any, args_t)>;
using equality_check_t = std::function<bool(flow::any, flow::any)>;

//
auto defaultMemoize = [](func_t func, std::vector<equality_check_t> equality_checks){
  
  auto last_args = std::make_shared<std::vector<flow::any>>(std::vector<flow::any>());
  auto last_result = std::make_shared<flow::any>(flow::any()); 

  return [last_args = last_args, last_result = last_result, func = func, equality_checks = equality_checks]
  (args_t args) -> flow::any{

    if ( last_args->size() == args.size() ){

      if (args.size() != equality_checks.size()){
        throw std::runtime_error("Size of args and equility_checks are mismatch.");
      }

      auto all_args_are_equal = true;
      for(int i = 0; i < args.size(); i++){
        auto arg_equal = equality_checks[i]( args[i], (*last_args)[i] );
        if (!arg_equal){ 
          all_args_are_equal = false; 
          break; 
        }
      }

      if (all_args_are_equal){ 
        std::cout << "use cache" << "\n";
        return *last_result; 
      }
    }

    auto new_result = func(args);

    *last_args = args;
    *last_result = new_result;
    return *last_result;
  };
};

auto create_selector_creator( std::vector<selector_t> selectors, func_t func, std::vector<equality_check_t> equality_checks) -> selector_t{

  auto memoizedResultFunc = defaultMemoize(
    [func = func](args_t args){
      std::cout << "recompute" << std::endl;
      return func(args);
    }, 
    equality_checks
  );

  auto selector = selector_t{ 
    [memoizedResultFunc = memoizedResultFunc, selectors = selectors](auto state, auto args){
      std::vector<flow::any> params;
      for (const auto selector: selectors) {
        auto param = selector(state, args);
        params.push_back(param);
      }      
      return memoizedResultFunc(params);
  }};

  return selector;
};


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
    [](flow::any state, auto args){
      return state.as<RootState>().id;
    }};

  auto sub_id_selector = selector_t{
    [](flow::any state, auto args){
      return state.as<RootState>().sub_state.sub_id;
    }};

  auto func = func_t{
    [](std::vector<flow::any> params){
      auto id = params.at(0).as<int>();
      auto sub_id = params.at(1).as<int>();
      return id * sub_id;
    }};

  auto int_equals = equality_check_t{ [](flow::any left, flow::any right){
    return left.as<int>() == right.as<int>();
  }};
  auto equality_checks = std::vector<equality_check_t>{int_equals, int_equals};

  auto root_state = RootState();
  root_state.id = 2;
  root_state.sub_state.sub_id = 4;

  auto multiply_selector = create_selector_creator({id_selector,sub_id_selector}, func, equality_checks);
  auto x = multiply_selector(root_state, {});
  std::cout << x.as<int>() << std::endl;

  root_state.sub_state.sub_id = 5;
  auto y = multiply_selector(root_state, {});
  std::cout << y.as<int>() << std::endl;

  root_state.sub_state.sub_id = 5;
  auto z = multiply_selector(root_state, {});
  std::cout << z.as<int>() << std::endl;

  root_state.id = 10;
  auto a = multiply_selector(root_state, {});
  std::cout << a.as<int>() << std::endl;

  auto b = multiply_selector(root_state, {});
  std::cout << b.as<int>() << std::endl;

  root_state.id = 10;
  auto c = multiply_selector(root_state, {});
  std::cout << c.as<int>() << std::endl;

  root_state.sub_state.sub_id = 4;
  auto d = multiply_selector(root_state, {});
  std::cout << d.as<int>() << std::endl;
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
  reselect_example();
  // simple_example();
  std::cout << "------------------------------" << std::endl;
  // thunk_middleware_example();
  return 0;
}
