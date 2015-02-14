/**	@file	Facilities for the module-bound allocator.
	A C++ allocator that frees memory in the module (shared module, executable) it was allocated in.

	@attention	The module-bound allocator is a *stateful* allocator, which 
	your STL library must support. Not all do, but to my current 
	knowledge MSVC 2003 and above and g++ do support them.

	A stateful allocator is an allocator storing information about state in 
	its member variables, in contrary to stateless allocators.
	The current C++ Standard imposes restrictions on user-defined allocators 
	in section 20.1.5/4-5:
	"4 Implementations of containers described in this International Standard 
	are permitted to assume that their Allocator template parameter meets the 
	following two additional requirements beyond those in Table 32.
	— All instances of a given allocator type are required to be 
	interchangeable and always compare equal to each other.
	— The typedef members pointer, const_pointer, size_type, and 
	difference_type are required to be T*, T const*, size_t, and ptrdiff_t, 
	respectively.
	5 Implementors are encouraged to supply libraries that can accept 
	allocators that encapsulate more general memory models and that support 
	nonequal instances. In such implementations, any requirements imposed on 
	allocators by containers beyond those requirements that appear in Table 32, 
	and the semantics of containers and algorithms when allocator instances 
	compare nonequal, are implementationdefined."

	This basically means that STL implementations are allowed not to support 
	user-defined memory models and/or are allowed to require allocators of 
	the same type to be interchangeable. The former is garanteed by the 
	module-bound allocator but the latter is not as two allocator instances 
	originating from two different modules are inequal.
	There is a request pending for c++09 to remove this restriction
	(see current wishlist: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2006/n2034.htm)

	You can see on std::list<>::splice() or std::basic_string<>::swap() 
	whether your STL library supports stateful allocators. If it does then the 
	containers' allocators are compared and different actions are taken 
	based on their (in)equality.

	@attention	Some STL libraries contain a bug in their using of allocators. 
	Specifically, they pass null pointers to the deallocate function, which is 
	explicitly forbidden by the Standard [20.1.5 Table 32].
	From boost's pool_allocator I know of the default RogueWave library (which 
	Borland C++ uses, Builder and command-line compiler ver. 5 and earlier) 
	and of STLport (with any compiler, ver. 4.0 and earlier).
	Because the module-bound allocator calls the global operator delete()
	or operator delete[]() this might not be a problem because they should 
	check for a null pointer but I didn't investigate, so your mileage may 
	vary.
	If you have some news on this issue or need a workaround then please 
	contact me, so that I can update the module-bound allocator!

	@date	2008 07 24	klaus triendl	created
 */

#ifndef KJ_MODULEBOUND_ALLOCATOR_H_INCLUDED
#define KJ_MODULEBOUND_ALLOCATOR_H_INCLUDED

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#  pragma once
#endif

#include <type_traits>
#include <new>	// operator new/operator delete
#include <memory>	// std::allocator
#include <utility>	// std::pair, std::move
#include "modulebound_allocator_fwddecl.h"


namespace kj
{

namespace detail
{

// metafunction to remove the reference and all extents from T
template<typename T>
struct remove_reference_and_all_extents
{
	typedef typename	std::remove_all_extents<
							typename std::remove_reference<T>::type
						>::type type;
};


// helper function to create a pair of raw allocator functions
inline	// force inline to prevent multiple function definitions in multiple translation units
std::pair<fp_raw_allocate_t, fp_raw_deallocate_t>
fetch_raw_operators(bool is_array_allocation)
{
	return
			std::make_pair(
				// capture operator new/operator delete;
				// must do a cast (value-cast) because operators are overloaded
				is_array_allocation ? fp_raw_allocate_t(::operator new[]) : fp_raw_allocate_t(::operator new), 
				is_array_allocation ? fp_raw_deallocate_t(::operator delete[]) : fp_raw_deallocate_t(::operator delete)
			);
}

}	// namespace detail


/**	@short	Base class for all module-bound allocators

	Makes the default stl allocator functionality available by deriving publicly 
	from it (though you shouldn't cast) and additionally stores the 
	raw allocation operators in an implementation defined manner.
	Offers the nested template class @c rebind, a constant whether the raw 
	allocation functions for arrays or for single objects should be used.

	For derived classes a default constructor that captures the raw allocation 
	operators is available as well as a copy constructor and assignment operator 
	from different types of module-bound allocators.

	Note that this allocator implements special copy/move semantics:
	The copy constructor doesn't copy the raw allocation functions from the 
	allocator to be copied but rather captures the raw allocation functions from 
	the current translation unit.
	The move constructor doesn't move the raw allocation functions from the 
	other allocator but rather copies them.
 */
template<typename T, typename RawAllocation>
class modulebound_allocator_base: 
	// pass possibly cv-qualified raw type of T to std::allocator
	public std::allocator<typename detail::remove_reference_and_all_extents<T>::type>
{
	typedef std::allocator<typename detail::remove_reference_and_all_extents<T>::type> base;
	typedef std::pair<fp_raw_allocate_t, fp_raw_deallocate_t> raw_operators;


	// stores operator new/operator delete
	raw_operators m_raw_operators;

public:
	///	Convert a @c modulebound_allocator<T> to a @c modulebound_allocator<U>, 
	///	preserve raw allocation type
	template<class U>
	struct rebind
	{
		typedef modulebound_allocator<U, RawAllocation> other;
	};

	///	Should we use the array allocation or the single object allocation functions?
	/// (is_array_allocation is true_type or false_type)
	typedef	std::integral_constant<
				bool, 
				(RawAllocation::value == raw_allocation_deduce) ? std::is_array<T>::value : RawAllocation::value
			> 
			is_array_allocation; 


protected:

	/**	@short	Default constructor captures the c++ runtime's raw allocation functions 
		available to the current translation unit.
	 */
	modulebound_allocator_base() throw(): 
		base(), 
		// capture operator new/operator delete
		m_raw_operators(detail::fetch_raw_operators(is_array_allocation::value))
	{}

	// ~modulebound_allocator_base() throw() = default;
	/**	@short	Copy construct from other modulebound_allocator_bases, 
		capture operator new/operator delete from current translations unit.
	 */
	modulebound_allocator_base(const modulebound_allocator_base& rOther) throw(): 
		base(rOther), 
		// capture operator new/operator delete
		m_raw_operators(detail::fetch_raw_operators(is_array_allocation::value))
	{}

	/**	@short	Assign from other modulebound_allocator_bases, 
		capture operator new/operator delete from current translations unit.
	 */
	modulebound_allocator_base& operator =(const modulebound_allocator_base& rOther) throw()
	{
		base::operator =(rOther);
		if (this != &rOther)
			m_raw_operators = detail::fetch_raw_operators(is_array_allocation::value);

		return *this;
	}

	/**	@short	Copy construct from other modulebound_allocator_bases of different types, 
		array/single object allocation type must match.
	 */
	template<typename U, typename RawAllocationU>
	modulebound_allocator_base(const modulebound_allocator_base<U, RawAllocationU>& rOther) throw(): 
		base(rOther), 
		// capture operator new/operator delete
		m_raw_operators(detail::fetch_raw_operators(is_array_allocation::value))
	{
#if (_MSC_VER >= 1600)	// c++0x
		static_assert(is_array_allocation::value == A_other::is_array_allocation::value, "raw allocation type mismatch (array/single object allocation)");
#else
		typedef modulebound_allocator_base<U, RawAllocationU> A_other;
		// static assert on matching array/single object allocation type
		// from boost: intentionally complex - simplification causes regressions
		typedef char type_must_be_complete[(is_array_allocation::value == A_other::is_array_allocation::value) ? 1 : -1];
		(void) sizeof(type_must_be_complete);
#endif
	}

	/**	@short	Assign from other modulebound_allocator_bases of different types, 
		array/single object allocation type must match.
	 */
	template<typename U, typename RawAllocationU>
	modulebound_allocator_base& operator =(const modulebound_allocator_base<U, RawAllocationU>& rOther) throw()
	{
		typedef modulebound_allocator_base<U, RawAllocationU> A_other;
#if (_MSC_VER >= 1600)	// c++0x
		static_assert(is_array_allocation::value == A_other::is_array_allocation::value, "raw allocation type mismatch (array/single object allocation)");
#else
		// static assert on matching array/single object allocation type
		// from boost: intentionally complex - simplification causes regressions
		typedef char type_must_be_complete[(is_array_allocation::value == A_other::is_array_allocation::value) ? 1 : -1];
		(void) sizeof(type_must_be_complete);
#endif

		base::operator =(rOther);
		if (this != static_cast<const void*>(&rOther))
			m_raw_operators = detail::fetch_raw_operators(is_array_allocation::value);

		return *this;
	}

#if (_MSC_VER >= 1600)	// move semantics
	/**	@short	Move construct from other modulebound_allocator_bases of different types, 
		copy operator new/operator delete from other allocator 
		(other allocator is not deprived of its state)
	 */
	modulebound_allocator_base(const modulebound_allocator_base&& rOther) throw(): 
		base(std::move(rOther)), 
		m_raw_operators(rOther.get_raw_operators())
	{}

	/**	@short	Move assign from other modulebound_allocator_bases of different types, 
		copy operator new/operator delete from other allocator
		(other allocator is not deprived of its state)
	 */
	modulebound_allocator_base& operator =(const modulebound_allocator_base&& rOther) throw()
	{
		base::operator =(std::move(rOther));
		if (this != &rOther)
			m_raw_operators = rOther.get_raw_operators();

		return *this;
	}

	/**	@short	Move construct from other modulebound_allocator_bases of different types, 
		array/single object allocation type must match, 
		copy operator new/operator delete from other allocator
		(other allocator is not deprived of its state)
	 */
	template<typename U, typename RawAllocationU>
	modulebound_allocator_base(const modulebound_allocator_base<U, RawAllocationU>&& rOther) throw(): 
		base(std::move(rOther)), 
		m_raw_operators(rOther.get_raw_operators())
	{
		typedef modulebound_allocator_base<U, RawAllocationU> A_other;
#if 0	// c++0x
		static_assert(is_array_allocation::value == A_other::is_array_allocation::value, "raw allocation type mismatch (array/single object allocation)");
#else
		// static assert on matching array/single object allocation type
		// from boost: intentionally complex - simplification causes regressions
		typedef char type_must_be_complete[(is_array_allocation::value == A_other::is_array_allocation::value) ? 1 : -1];
		(void) sizeof(type_must_be_complete);
#endif
	}

	/**	@short	Move assign from other modulebound_allocator_bases of different types, 
		array/single object allocation type must match, 
		copy operator new/operator delete from other allocator
		(other allocator is not deprived of its state)
	 */
	template<typename U, typename RawAllocationU>
	modulebound_allocator_base& operator =(const modulebound_allocator_base<U, RawAllocationU>&& rOther) throw()
	{
		typedef modulebound_allocator_base<U, RawAllocationU> A_other;
#if (_MSC_VER >= 1600)	// c++0x
		static_assert(is_array_allocation::value == A_other::is_array_allocation::value, "raw allocation type mismatch (array/single object allocation)");
#else
		// static assert on matching array/single object allocation type
		// from boost: intentionally complex - simplification causes regressions
		typedef char type_must_be_complete[(is_array_allocation::value == A_other::is_array_allocation::value) ? 1 : -1];
		(void) sizeof(type_must_be_complete);
#endif

		base::operator =(std::move(rOther));
		if (this != static_cast<const void*>(&rOther))
			m_raw_operators = rOther.get_raw_operators();

		return *this;
	}
#endif

public:
	/**	@short	Make the raw allocation functions available to the caller
	 */
	raw_operators get_raw_operators() const throw()
	{
		return m_raw_operators;
	}
};


/**	@short	Allocator that stores the c++ runtime's raw allocation 
	functions on construction, chooses the correct functions for 
	arrays and single objects, by default based on the template parameter 
	@c T.

	Because each module on windows platforms has its own address space if the 
	module is linked with the static c/c++ runtime resources like strings 
	cannot be allocated in one module and deallocated in another;
	this means that you cannot return e.g. a std::string from a DLL exported 
	function.

	This allocator fills this gaping hole by capturing the module's 
	allocation/deallocation functions at the point the allocator is constructed, 
	ensuring that memory is allocated and deallocated in the same address space.

	The second template parameter @c C_raw_allocation enables it to choose 
	between the allocator/deallocator functions used for single objects and for 
	an array of objects.
	By default the array type trait of the first template parameter @c T makes 
	the switch.
	The outcome of the @c C_raw_allocation's value is as follows: 
	  * @c raw_allocation_single: 
	    the allocation functions for single objects is used, even when rebinding
	  * @c raw_allocation_array: 
	    the allocation functions for an array of objects is used, even when 
		rebinding
	  * @c raw_allocation_deduce: 
	    the allocation functions to use should be deduced from @c T on all 
		binding levels (i.e. when instantiating the modulebound_allocator<T> 
		and when instantiaiting @c modulebound_allocator<T>::template rebind<U>)
	 IMHO deducing should be the default but the STL containers do always a 
	 rebinding based on the container's value_type (which is e.g. char instead 
	 of @c char[] for @c  std::basic_string); thus we would always end up with 
	 the single object functions even a char[] was specified for @c T.

	 This allocator is derived from the default STL allocator to preserve any other 
	 default behaviour other than the allocation itself (e.g. construction, max_size).

	 @code
	 // use array allocator for strings or vectors:
	 typedef kj::modulebound_allocator<char[]> my_array_allocator;
	 typedef std::basic_string<char, std::char_traits<char>, my_array_allocator> my_string;
	 typedef std::vector<char, my_array_allocator> my_vector;

	 // use single object allocator for maps
	 typedef kj::modulebound_allocator<std::pair<int, int> > my_node_allocator;
	 typedef std::map<int, int, std::less<int>, my_node_allocator> my_map;
	 @endcode

	 @attention	Because the msvc c++ compilers cannot test whether a class 
	 defines its own memory allocation functions, this allocator is not the 
	 right choice for classes coming along with their own 
	 operator new/operator delete.

	 @date	2008 07 24	kj	created
 */
template<typename T, typename RawAllocation>
class modulebound_allocator: public modulebound_allocator_base<T, RawAllocation>
{
	typedef modulebound_allocator_base<T, RawAllocation> base;

public:
	// bring base types into template resolution scope
	using base::pointer;
	using base::const_pointer;
	using base::size_type;
	using base::value_type;


public:
	modulebound_allocator() throw(): 
		base()
	{}

	// c++0x
	//~modulebound_allocator() throw() = default;
	//modulebound_allocator(const modulebound_allocator& rOther) throw() = default;
	//modulebound_allocator& operator =(const modulebound_allocator& rOther) throw() = default;

	/**	@short	Copy construct from other modulebound_allocators of different types, 
		array/single object allocation type must match.
	 */
	template<typename U, typename RawAllocationU>
	modulebound_allocator(const modulebound_allocator<U, RawAllocationU>& rOther) throw(): 
		base(rOther)
	{}

	/**	@short	Assign from other modulebound_allocators of different types, 
		array/single object allocation type must match.
	 */
	template<typename U, typename RawAllocationU>
	modulebound_allocator& operator =(const modulebound_allocator<U, RawAllocationU>& rOther) throw()
	{
		base::operator =(rOther);
		return *this;
	}

#if (_MSC_VER >= 1600)	// move semantics
	/**	@short	Move construct from other modulebound_allocators
	 */
	modulebound_allocator(const modulebound_allocator&& rOther) throw()
		: base(std::move(rOther))
	{}

	/**	@short	Move assign from other modulebound_allocators
	 */
	modulebound_allocator& operator =(const modulebound_allocator&& rOther) throw()
	{
		base::operator =(std::move(rOther));
		return *this;
	}

	/**	@short	Move construct from other modulebound_allocators of different types, 
		array/single object allocation type must match.
	 */
	template<typename U, typename RawAllocationU>
	modulebound_allocator(const modulebound_allocator<U, RawAllocationU>&& rOther) throw(): 
		base(std::move(rOther))
	{}

	/**	@short	Move assign from other modulebound_allocators of different types, 
		array/single object allocation type must match.
	 */
	template<typename U, typename RawAllocationU>
	modulebound_allocator& operator =(const modulebound_allocator<U, RawAllocationU>&& rOther) throw()
	{
		base::operator =(std::move(Other));
		return *this;
	}
#endif


	/**	@short	Allocate array of @e nCount elements
		@throw	@c std::bad_alloc
	 */
	pointer allocate(size_type nCount)
	{
		return 
			static_cast<pointer>(this->get_raw_operators().first(
				sizeof(value_type) * nCount
			));
	}

	/**	@short	Allocate array of @e nCount elements, ignore hint
		@throw	@c std::bad_alloc
		@note	The hint argument's type is not a simple void* but the 
		allocator's const_pointer, see section 20.4.1 "The default allocator" 
		of the C++ Standard.
	 */
	pointer allocate(size_type nCount, typename modulebound_allocator<void, RawAllocation>::const_pointer)
	{
		// forward to no-hint version
		return this->allocate(nCount);
	}

	/**	@short	Deallocate object at @e p, ignore size
		@note	A number of common STL libraries contain bugs in their using of 
		allocators. Specifically, they pass null pointers to the deallocate function, 
		which is explicitly forbidden by the Standard [20.1.5 Table 32].
	 */
	void deallocate(pointer p, size_type) throw()
	{
		this->get_raw_operators().second(p);
	}
};


/**	@short	Specialization for void.
 */
template<typename RawAllocation>
class modulebound_allocator<void, RawAllocation>: 
	public modulebound_allocator_base<void, RawAllocation>
{
	typedef modulebound_allocator_base<void, RawAllocation> base;


public:
	modulebound_allocator() throw(): 
		base()
	{}

	// c++0x
	//~modulebound_allocator() throw() = default;
	//modulebound_allocator(const modulebound_allocator& rOther) throw() = default;
	//modulebound_allocator& operator =(const modulebound_allocator& rOther) throw() = default;

	/**	@short	Copy construct from other modulebound_allocators of different types, 
		array/single object allocation type must match.
	 */
	template<typename U, typename RawAllocationU>
	modulebound_allocator(const modulebound_allocator<U, RawAllocationU>& rOther) throw(): 
		base(rOther)
	{}

	/**	@short	Assign from other modulebound_allocators of different types, 
		array/single object allocation type must match.
	 */
	template<typename U, typename RawAllocationU>
	modulebound_allocator& operator =(const modulebound_allocator<U, RawAllocationU>& rOther) throw()
	{
		base::operator =(rOther);
		return *this;
	}

#if (_MSC_VER >= 1600)	// move semantics
	/**	@short	Move construct from other modulebound_allocators
	 */
	modulebound_allocator(const modulebound_allocator&& rOther) throw()
		: base(std::move(rOther))
	{}

	/**	@short	Move assign from other modulebound_allocators
	 */
	modulebound_allocator& operator =(const modulebound_allocator&& rOther) throw()
	{
		base::operator =(std::move(rOther));
		return *this;
	}

	/**	@short	Move construct from other modulebound_allocators of different types, 
		array/single object allocation type must match.
	 */
	template<typename U, typename RawAllocationU>
	modulebound_allocator(const modulebound_allocator<U, RawAllocationU>&& rOther) throw(): 
		base(std::move(rOther))
	{}

	/**	@short	Move assign from other modulebound_allocators of different types, 
		array/single object allocation type must match.
	 */
	template<typename U, typename RawAllocationU>
	modulebound_allocator& operator =(const modulebound_allocator<U, RawAllocationU>&& rOther) throw()
	{
		base::operator =(std::move(Other));
		return *this;
	}
#endif
};


/**	@short	Test for allocator equality - raw allocation functions must be the same
			(storage allocated from each can be deallocated via the other)
 */
template<typename T, typename RawAllocation, typename U, typename RawAllocationU> inline
bool operator ==(const modulebound_allocator<T, RawAllocation>& rLeft, 
				 const modulebound_allocator<U, RawAllocationU>& rRight) throw()
{
	// compare raw allocation functions pair (forwarding to pair comparison operator)
	return rLeft.get_raw_operators() == rRight.get_raw_operators();
}

/**	@short	Test for allocator inequality - raw allocation functions must be the same
			(storage allocated from each can't be deallocated via the other)
 */
template<typename T, typename RawAllocation, typename U, typename RawAllocationU> inline
bool operator !=(const modulebound_allocator<T, RawAllocation>& rLeft, 
				 const modulebound_allocator<U, RawAllocationU>& rRight) throw()
{
	// compare raw allocation functions pair (forwarding to pair comparison operator)
	return rLeft.get_raw_operators() != rRight.get_raw_operators();
}


}	// namespace kj


#endif	// file guard
