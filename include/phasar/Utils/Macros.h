/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_UTILS_MACROS_H
#define PHASAR_UTILS_MACROS_H

#define PSR_FWD(...) ::std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

#if __cplusplus < 202002L
#define PSR_CONCEPT static constexpr bool
#else
#define PSR_CONCEPT concept
#endif

#define PSR_DECLARE_HAS_MEMBER_FN_OVERLOAD(TraitName, FName, ...)              \
  template <typename _Tp, typename Enable = void>                              \
  struct has_##TraitName : std::false_type {};                                 \
  template <typename _Tp>                                                      \
  struct has_##TraitName<                                                      \
      _Tp, std::void_t<decltype(std::declval<_Tp>().FName(__VA_ARGS__))>>      \
      : std::true_type {};                                                     \
  template <typename _Tp>                                                      \
  PSR_CONCEPT has_##TraitName##_v = has_##TraitName<_Tp>::value

#define PSR_DECLARE_HAS_STATIC_MEMBER_FN_OVERLOAD(TraitName, FName, ...)       \
  template <typename _Tp, typename Enable = void>                              \
  struct has_##TraitName : std::false_type {};                                 \
  template <typename _Tp>                                                      \
  struct has_##TraitName<                                                      \
      _Tp, std::void_t<decltype(std::remove_cv_t<_Tp>::FName(__VA_ARGS__))>>   \
      : std::true_type {};                                                     \
  template <typename _Tp>                                                      \
  PSR_CONCEPT has_##TraitName##_v = has_##TraitName<_Tp>::value

#define PSR_DECLARE_HAS_MEMBER_FN(FName, ...)                                  \
  PSR_DECLARE_HAS_MEMBER_FN_OVERLOAD(FName, FName, ##__VA_ARGS__)
#define PSR_DECLARE_HAS_STATIC_MEMBER_FN(FName, ...)                           \
  PSR_DECLARE_HAS_STATIC_MEMBER_FN_OVERLOAD(FName, FName, ##__VA_ARGS__)

#define PSR_DECLARE_HAS_MEMBER_VAR(FName)                                      \
  template <typename _Tp, typename Enable = void>                              \
  struct has_##FName : std::false_type {};                                     \
  template <typename _Tp>                                                      \
  struct has_##FName<_Tp, std::void_t<decltype(std::declval<_Tp>().FName)>>    \
      : std::true_type {};                                                     \
  template <typename _Tp> PSR_CONCEPT has_##FName##_v = has_##FName<_Tp>::value

#define PSR_DECLARE_HAS_MEMBER_TYPE(FName)                                     \
  template <typename _Tp, typename Enable = void>                              \
  struct has_##FName : std::false_type {};                                     \
  template <typename _Tp>                                                      \
  struct has_##FName<_Tp, std::void_t<typename std::remove_cv_t<_Tp>::FName>>  \
      : std::true_type {};                                                     \
  template <typename _Tp> PSR_CONCEPT has_##FName##_v = has_##FName<_Tp>::value

#define PSR_DECLARE_HAS_NONMEMBER_FN_OVERLOAD(TraitName, FName, ...)           \
  template <typename _Tp, typename Enable = void>                              \
  struct has_##TraitName : std::false_type {};                                 \
  template <typename _Tp>                                                      \
  struct has_##TraitName<                                                      \
      _Tp, std::void_t<decltype(FName(std::declval<_Tp>(), ##__VA_ARGS__))>>   \
      : std::true_type {};                                                     \
  template <typename _Tp>                                                      \
  PSR_CONCEPT has_##TraitName##_v = has_##TraitName<_Tp>::value

#define PSR_DECLARE_HAS_NONMEMBER_FN(FName, ...)                               \
  PSR_DECLARE_HAS_NONMEMBER_FN_OVERLOAD(FName, FName, ##__VA_ARGS__)

#if __cplusplus >= 202002L
#define PSR_CXX20_CONSTEXPR constexpr
#else
#define PSR_CXX20_CONSTEXPR inline
#endif

#if __cplusplus >= 202302L
#define PSR_CXX23_CONSTEXPR constexpr
#else
#define PSR_CXX23_CONSTEXPR inline
#endif

#endif // PHASAR_UTILS_MACROS_H
