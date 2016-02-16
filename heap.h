///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// This file implements heap functionality much like the std C++ heap algorithms.
// Such heaps are not the same thing as memory heaps or pools, but rather are
// semi-sorted random access containers which have the primary purpose of 
// supporting the implementation of priority_queue and similar data structures.
// 
// The primary distinctions between this heap functionality and std::heap are:
//    - This heap exposes some extra functionality such as isHeap and changeHeap.
//    - This heap is more efficient than versions found in typical STL 
//      implementations such as STLPort, Microsoft, and Metrowerks. This comes
//      about due to better use of array dereferencing and branch prediction.
//      You should expect of 5-30%, depending on the usage and platform.
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// The publicly usable functions we define are:
//    pushHeap     -- Adds an entry to a heap.                             Same as C++ std::pushHeap.
//    popHeap      -- Removes the top entry from a heap.                   Same as C++ std::popHeap.
//    makeHeap     -- Converts an array to a heap.                         Same as C++ std::makeHeap.
//    sortHeap     -- Sorts a heap in place.                               Same as C++ std::sortHeap.
//    removeHeap   -- Removes an arbitrary entry from a heap.
//    changeHeap   -- Changes the priority of an entry in the heap.
//    isHeap       -- Returns true if an array appears is in heap format.   Same as C++11 std::isHeap.
//    isHeap_until -- Returns largest part of the range which is a heap.    Same as C++11 std::isHeap_until.
///////////////////////////////////////////////////////////////////////////////



#ifndef EASTL_HEAP_H
#define EASTL_HEAP_H


#include <eastl/internal/config.h>
#include <eastl/iterator.h>
#include <stddef.h>

#if defined(EASTL_PRAGMA_ONCE_SUPPORTED)
	#pragma once // Some compilers (e.g. VC++) benefit significantly from using this. We've measured 3-4% build speed improvements in apps as a result.
#endif



namespace eastl
{

	///////////////////////////////////////////////////////////////////////
	// promoteHeap (internal function)
	///////////////////////////////////////////////////////////////////////

	/// promoteHeap
	///
	/// Moves a value in the heap from a given position upward until 
	/// it is sorted correctly. It's kind of like bubble-sort, except that
	/// instead of moving linearly from the back of a list to the front,
	/// it moves from the bottom of the tree up the branches towards the
	/// top. But otherwise is just like bubble-sort.
	///
	/// This function requires that the value argument refer to a value
	/// that is currently not within the heap.
	///
	template <typename RandomAccessIterator, typename Distance, typename T>
	inline void promoteHeap(RandomAccessIterator first, Distance topPosition, Distance position, const T& value)
	{
		for(Distance parentPosition = (position - 1) >> 1; // This formula assumes that (position > 0). // We use '>> 1' instead of '/ 2' because we have seen VC++ generate better code with >>.
			(position > topPosition) && (*(first + parentPosition) < value);
			parentPosition = (position - 1) >> 1)
		{
			*(first + position) = *(first + parentPosition); // Swap the node with its parent.
			position = parentPosition;
		}

		*(first + position) = value;
	}

	/// promoteHeap
	///
	/// Takes a Compare(a, b) function (or function object) which returns true if a < b.
	/// For example, you could use the standard 'less' comparison object.
	///
	/// The Compare function must work equivalently to the compare function used
	/// to make and maintain the heap.
	///
	/// This function requires that the value argument refer to a value
	/// that is currently not within the heap.
	///
	template <typename RandomAccessIterator, typename Distance, typename T, typename Compare>
	inline void promoteHeap(RandomAccessIterator first, Distance topPosition, Distance position, const T& value, Compare compare)
	{
		for(Distance parentPosition = (position - 1) >> 1; // This formula assumes that (position > 0). // We use '>> 1' instead of '/ 2' because we have seen VC++ generate better code with >>.
			(position > topPosition) && compare(*(first + parentPosition), value);
			parentPosition = (position - 1) >> 1)
		{
			*(first + position) = *(first + parentPosition); // Swap the node with its parent.
			position = parentPosition;
		}

		*(first + position) = value;
	}



	///////////////////////////////////////////////////////////////////////
	// adjustHeap (internal function)
	///////////////////////////////////////////////////////////////////////

	/// adjustHeap
	///
	/// Given a position that has just been vacated, this function moves
	/// new values into that vacated position appropriately. The value
	/// argument is an entry which will be inserted into the heap after
	/// we move nodes into the positions that were vacated.
	///
	/// This function requires that the value argument refer to a value
	/// that is currently not within the heap.
	///
	template <typename RandomAccessIterator, typename Distance, typename T>
	void adjustHeap(RandomAccessIterator first, Distance topPosition, Distance heapSize, Distance position, const T& value)
	{
		// We do the conventional approach of moving the position down to the 
		// bottom then inserting the value at the back and moving it up.
		Distance childPosition = (2 * position) + 2;

		for(; childPosition < heapSize; childPosition = (2 * childPosition) + 2)
		{
			if(*(first + childPosition) < *(first + (childPosition - 1))) // Choose the larger of the two children.
				--childPosition;
			*(first + position) = *(first + childPosition); // Swap positions with this child.
			position = childPosition;
		}

		if(childPosition == heapSize) // If we are at the very last index of the bottom...
		{
			*(first + position) = *(first + (childPosition - 1));
			position = childPosition - 1;
		}

		eastl::promoteHeap<RandomAccessIterator, Distance, T>(first, topPosition, position, value);
	}


	/// adjustHeap
	///
	/// The Compare function must work equivalently to the compare function used
	/// to make and maintain the heap.
	///
	/// This function requires that the value argument refer to a value
	/// that is currently not within the heap.
	///
	template <typename RandomAccessIterator, typename Distance, typename T, typename Compare>
	void adjustHeap(RandomAccessIterator first, Distance topPosition, Distance heapSize, Distance position, const T& value, Compare compare)
	{
		// We do the conventional approach of moving the position down to the 
		// bottom then inserting the value at the back and moving it up.
		Distance childPosition = (2 * position) + 2;

		for(; childPosition < heapSize; childPosition = (2 * childPosition) + 2)
		{
			if(compare(*(first + childPosition), *(first + (childPosition - 1)))) // Choose the larger of the two children.
				--childPosition;
			*(first + position) = *(first + childPosition); // Swap positions with this child.
			position = childPosition;
		}

		if(childPosition == heapSize) // If we are at the bottom...
		{
			*(first + position) = *(first + (childPosition - 1));
			position = childPosition - 1;
		}

		eastl::promoteHeap<RandomAccessIterator, Distance, T, Compare>(first, topPosition, position, value, compare);
	}




	///////////////////////////////////////////////////////////////////////
	// pushHeap
	///////////////////////////////////////////////////////////////////////

	/// pushHeap
	///
	/// Adds an item to a heap (which is an array). The item necessarily
	/// comes from the back of the heap (array). Thus, the insertion of a 
	/// new item in a heap is a two step process: pushBack and pushHeap.
	///
	/// Example usage:
	///    vector<int> heap;
	///    
	///    heap.pushBack(3);
	///    pushHeap(heap.begin(), heap.end()); // Places '3' appropriately.
	///
	template <typename RandomAccessIterator>
	inline void pushHeap(RandomAccessIterator first, RandomAccessIterator last)
	{
		typedef typename eastl::iterator_traits<RandomAccessIterator>::difference_type difference_type;
		typedef typename eastl::iterator_traits<RandomAccessIterator>::value_type      value_type;

		const value_type tempBottom(*(last - 1));

		eastl::promoteHeap<RandomAccessIterator, difference_type, value_type>
						   (first, (difference_type)0, (difference_type)(last - first - 1), tempBottom);
	}


	/// pushHeap
	///
	/// This version is useful for cases where your object comparison is unusual 
	/// or where you want to have the heap store pointers to objects instead of 
	/// storing the objects themselves (often in order to improve cache coherency
	/// while doing sorting).
	///
	/// The Compare function must work equivalently to the compare function used
	/// to make and maintain the heap.
	///
	template <typename RandomAccessIterator, typename Compare>
	inline void pushHeap(RandomAccessIterator first, RandomAccessIterator last, Compare compare)
	{
		typedef typename eastl::iterator_traits<RandomAccessIterator>::difference_type difference_type;
		typedef typename eastl::iterator_traits<RandomAccessIterator>::value_type      value_type;

		const value_type tempBottom(*(last - 1));

		eastl::promoteHeap<RandomAccessIterator, difference_type, value_type, Compare>
						   (first, (difference_type)0, (difference_type)(last - first - 1), tempBottom, compare);
	}




	///////////////////////////////////////////////////////////////////////
	// popHeap
	///////////////////////////////////////////////////////////////////////

	/// popHeap
	///
	/// Removes the first item from the heap (which is an array), and adjusts
	/// the heap so that the highest priority item becomes the new first item.
	///
	/// Example usage:
	///    vector<int> heap;
	///    
	///    heap.pushBack(2);
	///    heap.pushBack(3);
	///    heap.pushBack(1);
	///    <use heap[0], which is the highest priority item in the heap>
	///    popHeap(heap.begin(), heap.end());  // Moves heap[0] to the back of the heap and adjusts the heap.
	///    heap.popBack();                     // Remove value that was just at the top of the heap
	///
	template <typename RandomAccessIterator>
	inline void popHeap(RandomAccessIterator first, RandomAccessIterator last)
	{
		typedef typename eastl::iterator_traits<RandomAccessIterator>::difference_type difference_type;
		typedef typename eastl::iterator_traits<RandomAccessIterator>::value_type      value_type;

		const value_type tempBottom(*(last - 1));
		*(last - 1) = *first;
		eastl::adjustHeap<RandomAccessIterator, difference_type, value_type>
						  (first, (difference_type)0, (difference_type)(last - first - 1), 0, tempBottom);
	}



	/// popHeap
	///
	/// This version is useful for cases where your object comparison is unusual 
	/// or where you want to have the heap store pointers to objects instead of 
	/// storing the objects themselves (often in order to improve cache coherency
	/// while doing sorting).
	///
	/// The Compare function must work equivalently to the compare function used
	/// to make and maintain the heap.
	///
	template <typename RandomAccessIterator, typename Compare>
	inline void popHeap(RandomAccessIterator first, RandomAccessIterator last, Compare compare)
	{
		typedef typename eastl::iterator_traits<RandomAccessIterator>::difference_type difference_type;
		typedef typename eastl::iterator_traits<RandomAccessIterator>::value_type      value_type;

		const value_type tempBottom(*(last - 1));
		*(last - 1) = *first;
		eastl::adjustHeap<RandomAccessIterator, difference_type, value_type, Compare>
						  (first, (difference_type)0, (difference_type)(last - first - 1), 0, tempBottom, compare);
	}




	///////////////////////////////////////////////////////////////////////
	// makeHeap
	///////////////////////////////////////////////////////////////////////


	/// makeHeap
	///
	/// Given an array, this function converts it into heap format.
	/// The complexity is O(n), where n is count of the range.
	/// The input range is not required to be in any order.
	///
	template <typename RandomAccessIterator>
	void makeHeap(RandomAccessIterator first, RandomAccessIterator last)
	{
		// We do bottom-up heap construction as per Sedgewick. Such construction is O(n).
		typedef typename eastl::iterator_traits<RandomAccessIterator>::difference_type difference_type;
		typedef typename eastl::iterator_traits<RandomAccessIterator>::value_type      value_type;

		const difference_type heapSize = last - first;

		if(heapSize >= 2) // If there is anything to do... (we need this check because otherwise the math fails below).
		{
			difference_type parentPosition = ((heapSize - 2) >> 1) + 1; // We use '>> 1' instead of '/ 2' because we have seen VC++ generate better code with >>.

			do{
				--parentPosition;
				const value_type temp(*(first + parentPosition));
				eastl::adjustHeap<RandomAccessIterator, difference_type, value_type>
								  (first, parentPosition, heapSize, parentPosition, temp);
			} while(parentPosition != 0);
		}
	}


	template <typename RandomAccessIterator, typename Compare>
	void makeHeap(RandomAccessIterator first, RandomAccessIterator last, Compare compare)
	{
		typedef typename eastl::iterator_traits<RandomAccessIterator>::difference_type difference_type;
		typedef typename eastl::iterator_traits<RandomAccessIterator>::value_type      value_type;

		const difference_type heapSize = last - first;

		if(heapSize >= 2) // If there is anything to do... (we need this check because otherwise the math fails below).
		{
			difference_type parentPosition = ((heapSize - 2) >> 1) + 1; // We use '>> 1' instead of '/ 2' because we have seen VC++ generate better code with >>.

			do{
				--parentPosition;
				const value_type temp(*(first + parentPosition));
				eastl::adjustHeap<RandomAccessIterator, difference_type, value_type, Compare>
								  (first, parentPosition, heapSize, parentPosition, temp, compare);
			} while(parentPosition != 0);
		}
	}




	///////////////////////////////////////////////////////////////////////
	// sortHeap
	///////////////////////////////////////////////////////////////////////

	/// sortHeap
	///
	/// After the application if this algorithm, the range it was applied to 
	/// is no longer a heap, though it will be a reverse heap (smallest first).
	/// The item with the lowest priority will be first, and the highest last.
	/// This is not a stable sort because the relative order of equivalent 
	/// elements is not necessarily preserved.
	/// The range referenced must be valid; all pointers must be dereferenceable 
	/// and within the sequence the last position is reachable from the first 
	/// by incrementation.
	/// The complexity is at most O(n * log(n)), where n is count of the range.
	///
	template <typename RandomAccessIterator>
	inline void sortHeap(RandomAccessIterator first, RandomAccessIterator last)
	{
		for(; (last - first) > 1; --last) // We simply use the heap to sort itself.
			eastl::popHeap<RandomAccessIterator>(first, last);
	}


	/// sortHeap
	///
	/// The Compare function must work equivalently to the compare function used
	/// to make and maintain the heap.
	///
	template <typename RandomAccessIterator, typename Compare>
	inline void sortHeap(RandomAccessIterator first, RandomAccessIterator last, Compare compare)
	{
		for(; (last - first) > 1; --last) // We simply use the heap to sort itself.
			eastl::popHeap<RandomAccessIterator, Compare>(first, last, compare);
	}



	///////////////////////////////////////////////////////////////////////
	// removeHeap
	///////////////////////////////////////////////////////////////////////

	/// removeHeap
	///
	/// Removes an arbitrary entry from the heap and adjusts the heap appropriately.
	/// This function is unlike popHeap in that popHeap moves the top item
	/// to the back of the heap, whereas removeHeap moves an arbitrary item to
	/// the back of the heap.
	///
	/// Note: Since this function moves the element to the back of the heap and 
	/// doesn't actually remove it from the given container, the user must call
	/// the container erase function if the user wants to erase the element 
	/// from the container.
	///
	template <typename RandomAccessIterator, typename Distance>
	inline void removeHeap(RandomAccessIterator first, Distance heapSize, Distance position)
	{
		typedef typename eastl::iterator_traits<RandomAccessIterator>::difference_type difference_type;
		typedef typename eastl::iterator_traits<RandomAccessIterator>::value_type      value_type;

		const value_type tempBottom(*(first + heapSize - 1));
		*(first + heapSize - 1) = *(first + position);
		eastl::adjustHeap<RandomAccessIterator, difference_type, value_type>
						  (first, (difference_type)0, (difference_type)(heapSize - 1), (difference_type)position, tempBottom);
	}


	/// removeHeap
	///
	/// The Compare function must work equivalently to the compare function used
	/// to make and maintain the heap.
	///
	/// Note: Since this function moves the element to the back of the heap and 
	/// doesn't actually remove it from the given container, the user must call
	/// the container erase function if the user wants to erase the element 
	/// from the container.
	///
	template <typename RandomAccessIterator, typename Distance, typename Compare>
	inline void removeHeap(RandomAccessIterator first, Distance heapSize, Distance position, Compare compare)
	{
		typedef typename eastl::iterator_traits<RandomAccessIterator>::difference_type difference_type;
		typedef typename eastl::iterator_traits<RandomAccessIterator>::value_type      value_type;

		const value_type tempBottom(*(first + heapSize - 1));
		*(first + heapSize - 1) = *(first + position);
		eastl::adjustHeap<RandomAccessIterator, difference_type, value_type, Compare>
						  (first, (difference_type)0, (difference_type)(heapSize - 1), (difference_type)position, tempBottom, compare);
	}



	///////////////////////////////////////////////////////////////////////
	// changeHeap
	///////////////////////////////////////////////////////////////////////

	/// changeHeap
	///
	/// Given a value in the heap that has changed in priority, this function
	/// adjusts the heap appropriately. The heap size remains unchanged after
	/// this operation. 
	///
	template <typename RandomAccessIterator, typename Distance>
	inline void changeHeap(RandomAccessIterator first, Distance heapSize, Distance position)
	{
		typedef typename eastl::iterator_traits<RandomAccessIterator>::difference_type difference_type;
		typedef typename eastl::iterator_traits<RandomAccessIterator>::value_type      value_type;

		eastl::removeHeap<RandomAccessIterator, Distance>(first, heapSize, position);

		value_type tempBottom(*(first + heapSize - 1));

		eastl::promoteHeap<RandomAccessIterator, difference_type, value_type>
						   (first, (difference_type)0, (difference_type)(heapSize - 1), tempBottom);
	}


	/// changeHeap
	///
	/// The Compare function must work equivalently to the compare function used
	/// to make and maintain the heap.
	///
	template <typename RandomAccessIterator, typename Distance, typename Compare>
	inline void changeHeap(RandomAccessIterator first, Distance heapSize, Distance position, Compare compare)
	{
		typedef typename eastl::iterator_traits<RandomAccessIterator>::difference_type difference_type;
		typedef typename eastl::iterator_traits<RandomAccessIterator>::value_type      value_type;

		eastl::removeHeap<RandomAccessIterator, Distance, Compare>(first, heapSize, position, compare);

		value_type tempBottom(*(first + heapSize - 1));

		eastl::promoteHeap<RandomAccessIterator, difference_type, value_type, Compare>
						   (first, (difference_type)0, (difference_type)(heapSize - 1), tempBottom, compare);
	}



	///////////////////////////////////////////////////////////////////////
	// isHeap_until
	///////////////////////////////////////////////////////////////////////

	/// isHeap_until
	///
	template <typename RandomAccessIterator>
	inline RandomAccessIterator isHeap_until(RandomAccessIterator first, RandomAccessIterator last)
	{
		int counter = 0;

		for(RandomAccessIterator child = first + 1; child < last; ++child, counter ^= 1)
		{
			if(*first < *child)  // We must use operator <, and are not allowed to use > or >= here.
				return child;
			first += counter; // counter switches between 0 and 1 every time through.
		}

		return last;
	}


	/// isHeap_until
	///
	/// The Compare function must work equivalently to the compare function used
	/// to make and maintain the heap.
	///
	template <typename RandomAccessIterator, typename Compare>
	inline RandomAccessIterator isHeap_until(RandomAccessIterator first, RandomAccessIterator last, Compare compare)
	{
		int counter = 0;

		for(RandomAccessIterator child = first + 1; child < last; ++child, counter ^= 1)
		{
			if(compare(*first, *child))
				return child;
			first += counter; // counter switches between 0 and 1 every time through.
		}

		return last;
	}



	///////////////////////////////////////////////////////////////////////
	// isHeap
	///////////////////////////////////////////////////////////////////////

	/// isHeap
	///
	/// This is a useful debugging algorithm for verifying that a random  
	/// access container is in heap format. 
	///
	template <typename RandomAccessIterator>
	inline bool isHeap(RandomAccessIterator first, RandomAccessIterator last)
	{
		return (eastl::isHeap_until(first, last) == last);
	}


	/// isHeap
	///
	/// The Compare function must work equivalently to the compare function used
	/// to make and maintain the heap.
	///
	template <typename RandomAccessIterator, typename Compare>
	inline bool isHeap(RandomAccessIterator first, RandomAccessIterator last, Compare compare)
	{
		return (eastl::isHeap_until(first, last, compare) == last);
	}


	// To consider: The following may be a faster implementation for most cases.
	//
	// template <typename RandomAccessIterator>
	// inline bool isHeap(RandomAccessIterator first, RandomAccessIterator last)
	// {
	//     if(((uintptr_t)(last - first) & 1) == 0) // If the range has an even number of elements...
	//         --last;
	// 
	//     RandomAccessIterator parent = first, child = (first + 1);
	// 
	//     for(; child < last; child += 2, ++parent)
	//     {
	//         if((*parent < *child) || (*parent < *(child + 1)))
	//             return false;
	//     }
	// 
	//     if((((uintptr_t)(last - first) & 1) == 0) && (*parent < *child))
	//         return false;
	// 
	//     return true;
	// }


} // namespace eastl


#endif // Header include guard




