#pragma once

// #define RESELECT_DEBUG

namespace flow{


	// Tuple iterator helper
	template<std::size_t I = 0, typename FuncT, typename... Tp>
	inline typename std::enable_if<I == sizeof...(Tp), void>::type
	  for_each_in_tuple(std::tuple<Tp...> &, FuncT) // Unused arguments are given no names.
	  { }

	template<std::size_t I = 0, typename FuncT, typename... Tp>
	inline typename std::enable_if<I < sizeof...(Tp), void>::type
	  for_each_in_tuple(std::tuple<Tp...>& t, FuncT f)
	  {
	    f(std::get<I>(t));
	    for_each_in_tuple<I + 1, FuncT, Tp...>(t, f);
	  }

	template<std::size_t I = 0, typename FuncT, typename... Tp, typename... Sp>
	inline typename std::enable_if<I == sizeof...(Tp), void>::type
	  for_each_in_tuple(std::tuple<Tp...> &, std::tuple<Sp...>& s, FuncT) // Unused arguments are given no names.
	  { }

	template<std::size_t I = 0, typename FuncT, typename... Tp, typename... Sp>
	inline typename std::enable_if<I < sizeof...(Tp), void>::type
	  for_each_in_tuple(std::tuple<Tp...>& t, std::tuple<Sp...>& s, FuncT f)
	  {
	    f(std::get<I>(t), std::get<I>(s));
	    for_each_in_tuple<I + 1, FuncT, Tp...>(t, s, f);
	  }

	template<std::size_t I = 0, typename FuncT, typename... Tp, typename... Sp, typename... Xp>
	inline typename std::enable_if<I == sizeof...(Tp), void>::type
	  for_each_in_tuple(std::tuple<Tp...> &, std::tuple<Sp...>& s, std::tuple<Xp...>& x, FuncT) // Unused arguments are given no names.
	  { }

	template<std::size_t I = 0, typename FuncT, typename... Tp, typename... Sp, typename... Xp>
	inline typename std::enable_if<I < sizeof...(Tp), void>::type
	  for_each_in_tuple(std::tuple<Tp...>& t, std::tuple<Sp...>& s, std::tuple<Xp...>& x, FuncT f)
	  {
	    f(std::get<I>(t), std::get<I>(s), std::get<I>(x));
	    for_each_in_tuple<I + 1, FuncT, Tp...>(t, s, x, f);
	  }


	template <typename T, typename... Args>
	using result_func = std::function<T(std::tuple<Args...>)>;

	template <typename S, typename T>
	using selector = std::function<T(S)>;

	template <typename T, typename... Args>
	using memoize_func = std::function<T(std::tuple<Args...>)>;

	// Map memoize
	template <typename S>
	using map_string_key = std::function<std::string(S)>;

	template <typename T, typename Keys, typename... Args>
	auto map_memoize = [](Keys keys) -> std::function<memoize_func<T, Args...>(result_func<T, Args...>)>{
	  return [=](result_func<T, Args...> func){
	    auto result_map = std::unordered_map<std::string, T>{};
	    return [=](std::tuple<Args...> args) mutable -> T{

	      auto key = std::string();
	      for_each_in_tuple(keys, args, [&key = key](auto key_func, auto arg){
	        key = key + key_func(arg);
	      });

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


	template <typename T>
	using equality_check = std::function<bool(T, T)>;

	template <typename T, typename EqualityChecks, typename... Args>
	auto default_memoize = [](EqualityChecks equality_checks) -> std::function<memoize_func<T, Args...>(result_func<T, Args...>)>{
	  return [=](result_func<T, Args...> func){

	    auto last_args = std::tuple<Args...>();
	    auto last_result = T();

	    return [=](std::tuple<Args...> args) mutable -> T{

	      auto all_args_are_equal = true;

	      for_each_in_tuple(args, last_args, equality_checks, [&all_args_are_equal = all_args_are_equal](auto arg, auto last_arg, auto equality_check){
	        auto arg_equal = equality_check(arg, last_arg);
	        if (!arg_equal){
	          all_args_are_equal = false; 
	        }
	      });

	      if (all_args_are_equal){ 
					#ifdef RESELECT_DEBUG
	        std::cout << "use cache" << "\n";
					#endif
	        return last_result; 
	      }

	      auto new_result = func(args);
	      last_args = args;
	      last_result = new_result;
	      return last_result;
	    };
	  };
	};

	template <std::size_t ...I, typename Params, typename Selectors, typename State>
	void copy_params_result_impl(Params &params, Selectors const & selectors, State const & state, std::index_sequence<I...>){
	    int dummy[] = { (std::get<I>(params) = std::get<I>(selectors)(state), 0)... };
	    static_cast<void>(dummy);
	}

	template <typename Params, typename Selectors, typename State>
	void copy_params_result(Params &params, Selectors const & selectors, State const & state){
	  auto index_sequence = std::make_index_sequence<std::tuple_size<Params>::value>();
	  copy_params_result_impl( params, selectors, state, index_sequence);
	}

	template <typename S, typename T, typename Selectors, typename... Args>
	auto create_selector_creator(memoize_func<T, Args...> memoized_result_func) -> std::function<selector<S,T>(Selectors, result_func<T, Args...>)>{
	  return [=](auto selectors, auto func){
	    return [=](auto state){
	        auto params = std::tuple<Args...>();
	        copy_params_result(params, selectors, state);
	        return memoized_result_func(params);
	    };
	  };
	};

	template <typename S, typename T, typename Selectors, typename... Args>
	auto create_selector(
	        Selectors selectors, result_func<T, Args...> func, 
	        std::function<memoize_func<T, Args...>(result_func<T, Args...>)> memoize) -> selector<S,T>{

	#ifdef RESELECT_DEBUG
	  auto memoized_result_func = memoize(
	    [func = func](std::tuple<Args...> args){
	      std::cout << "recompute" << std::endl;
	      return func(args);
	    });
	#else
	  auto memoized_result_func = memoize(func);
	#endif
	  return create_selector_creator<S, T, Selectors, Args...>(memoized_result_func)(selectors, func);
	}


}
