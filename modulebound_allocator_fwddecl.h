/**	@file	Forward declarations for the module-bound allocator.
	@date	2008 07 24	klaus triendl	created
 */

#ifndef KJ_MODULEBOUND_ALLOCATOR_FWDDECL_H_INCLUDED
#define KJ_MODULEBOUND_ALLOCATOR_FWDDECL_H_INCLUDED

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#  pragma once
#endif

#include <type_traits>
#include <stddef.h>


namespace kj
{

#ifdef _MSC_VER	// msvc declares operator new/operator delete with __cdecl calling convension

///	function pointer type for raw memory allocation functions
typedef void* (__cdecl *fp_raw_allocate_t)(size_t);
///	function pointer type for raw memory deallocation functions
typedef void (__cdecl *fp_raw_deallocate_t)(void*);

#else	// use default for other compilers

///	function pointer type for raw memory allocation functions
typedef void* (*fp_raw_allocate_t)(size_t);
///	function pointer type for raw memory deallocation functions
typedef void (*fp_raw_deallocate_t)(void*);

#endif



/**	@short	Named constants telling us whether to use the raw allocation 
	functions for an array of objects or for single objects 
	(operator new[]()/operator delete[](), operator new()/operator delete() 
	respectively).
 */
enum raw_allocation_type
{
	// use raw single object operators
	raw_allocation_single = 0, 
	// use raw array operators
	raw_allocation_array = 1, 
	// deduce raw allocation type on all rebinding levels from template type;
	// this is useful when using the allocator's rebind<> structure 
	// (stl-containers do use rebind).
	// e.g. when you have a modulebound_allocator for an array of integers 
	// (modulebound_allocator<int[]> then the allocator uses array new/delete.
	// now, when rebinding, the raw_allocation_type determines whether the 
	// rebound allocator uses array new/delete as well or deduces the 
	// new/delete operator from the template parameter passed to rebind:
	// 
	//	modulebound_allocator<
	//		int[], 
	//		raw_allocation_variant<raw_allocation_array>
	//	>::rebind<int>::other;
	//	=> modulebound_allocator<int, raw_allocation_variant<raw_allocation_array>>;
	//	   (using array new/delete)
	// 
	//	modulebound_allocator<
	//		int[], 
	//		raw_allocation_variant<raw_allocation_deduce>, 
	//	>::rebind<int>::other;
	//	=> modulebound_allocator<int, raw_allocation_variant<raw_allocation_deduce>>;
	//	   (using single-object new/delete)
	raw_allocation_deduce = 2
};


#if 0	// c++0x (template aliases)
template<raw_allocation_type C>
using raw_allocation_variant =	std::integral_constant<
									raw_allocation_type, 
									C
								>;


// fwd decl of modulebound_allocator<T>;
// determine array/single allocation variant from T, use it for all rebinding levels
template<
	typename T, 
	typename RawAllocation =	raw_allocation_variant<
									std::is_array<T>::value ? 
										raw_allocation_array : 
										raw_allocation_single
								>
>
class modulebound_allocator;

#else

// fwd decl of modulebound_allocator<T>;
// determine array/single allocation variant from T, use it for all rebinding levels
template<
	typename T, 
	typename RawAllocation =	std::integral_constant<
									raw_allocation_type, 
									std::is_array<T>::value ? 
										raw_allocation_array : 
										raw_allocation_single
								>
>
class modulebound_allocator;

#endif	// c++0x


}	// namespace kj


#endif	// file guard
