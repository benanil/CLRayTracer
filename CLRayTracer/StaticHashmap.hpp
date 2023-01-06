#pragma once
// inspired from 
// Game Engine Architecture book page 456 and Facebook Folly F12Map
#pragma once
#include "Common.hpp"
#include <utility>
#include <cassert>

namespace Hasher
{
	template <typename T> inline uint Hash(const T& val) { return 0u; }
	template <> inline uint Hash(const int& in) { return (uint)in; }
}

// const size, non growable
// all operations is O(1), and memory allocation, initialization is also fast,
// chace frendly stack allocated contigues memory
template<typename Key, typename Value, int Size = 90, char BucketSize = 8>
class StaticHashMap
{
private:
	char numElements[Size]{ 0 };
	Key keys[Size * BucketSize]{ };
	mutable Value arr[Size * BucketSize]{ };
public:

	StaticHashMap() { }

	~StaticHashMap() { }

	StaticHashMap(const StaticHashMap& other) // copy constructor
	{
		ArrayCpy(numElements, other.numElements, BucketSize);
		ArrayCpy(arr, other.arr, Size * BucketSize);
	}

	StaticHashMap(const StaticHashMap&& other) // move constructor, we cant move just copy
	{
		ArrayMove(numElements, other.numElements, BucketSize);
		ArrayMove(arr, other.arr, Size * BucketSize);
	}

public:

	inline void Insert(const Key& key, Value&& value)
	{
		uint bucketIndex = Hasher::Hash(key) % Size;
		uint arrIndex = bucketIndex * BucketSize + (numElements[bucketIndex]++);
		assert(numElements[bucketIndex] <= BucketSize); // BucketSize is small
		keys[arrIndex] = key;
		arr[arrIndex] = std::move(value);
	}

	// returns true if removed correctly
	bool Remove(const Key& key)
	{
		const uint bucketIndex = Hasher::Hash(key) % Size;
		const uint  valueIndex = bucketIndex * BucketSize;
		const uint8 bucketSize = numElements[bucketIndex];

		for (uint8 i = 0; i < bucketSize; ++i)
		{
			if (keys[valueIndex + i] == key)
			{
				arr[valueIndex + i] = arr[--numElements[bucketIndex]];
				return true;
			}
		}
		return false;
	}

	bool Contains(const Key& key) const
	{
		const uint bucketIndex = Hasher::Hash(key) % Size;
		const uint  valueIndex = bucketIndex * BucketSize;
		const uint8 bucketSize = numElements[bucketIndex];

		for (uint8 i = 0; i < bucketSize; ++i)
			if (keys[valueIndex + i] == key) return true;

		return false;
	}

	Value* Find(const Key& key)
	{
		const uint bucketIndex = Hasher::Hash(key) % Size;
		const uint  valueIndex = bucketIndex * BucketSize;
		uint8 currBucketIndex = numElements[bucketIndex];

		while (currBucketIndex--)
			if (keys[valueIndex + currBucketIndex] == key)
				return &arr[valueIndex + currBucketIndex];

		return nullptr;
	}

	Value* FindOrCreate(const Key& key)
	{
		const uint bucketIndex = Hasher::Hash(key) % Size;
		uint  valueIndex = bucketIndex * BucketSize;
		uint8 currBucketIndex = numElements[bucketIndex];

		while (currBucketIndex--)
			if (keys[valueIndex + currBucketIndex] == key)
				return &arr[valueIndex + currBucketIndex];

		valueIndex += numElements[bucketIndex]++;

		assert(numElements[bucketIndex] <= BucketSize); // BucketSize is small

		keys[valueIndex] = key;
		arr[valueIndex] = Value();
		return &arr[valueIndex];
	}

	inline bool Empty() const
	{
		int currentBucket = Size;
		while (currentBucket--) if (numElements[currentBucket]) return true;
		return false;
	}

	char Count(const Key& key) const
	{
		const uint bucketIndex = Hasher::Hash(key) % Size;
		const uint  valueIndex = bucketIndex * BucketSize;
		const uint8 bucketSize = numElements[bucketIndex];
		char count = 0;

		for (uint8 i = 0; i < bucketSize; ++i)
			count += keys[valueIndex + i] == key;

		return count;
	}

	char CountBucket(const Key& key) const
	{
		return numElements[Hasher::Hash(key) % Size];
	}

	template<typename Callable_t>
	void Iterate(const Callable_t& func)
	{
		int currentBucket = Size;
		while (currentBucket--)
			for (int i = 0; i < numElements[currentBucket]; ++i)
				func(arr[currentBucket * BucketSize + i]);
	}

	void Clear()
	{
		int currentBucket = Size;
		while (currentBucket--)
		{
			while (numElements[currentBucket]--)
			{
				arr[currentBucket * BucketSize + numElements[currentBucket]].~Value();
			}
			numElements[currentBucket] = 0;
		}
	}

	void Reload()
	{
		int currentBucket = Size;
		while (currentBucket--)
		{
			while (numElements[currentBucket]--)
			{
				arr[currentBucket * BucketSize + numElements[currentBucket]].~Value();
				arr[currentBucket * BucketSize + numElements[currentBucket]] = Value();
			}
			numElements[currentBucket] = 0;
		}
	}

	// calls default constructor for each element
	inline void Initialize()
	{
		for (int i = 0; i < Size * BucketSize; ++i) arr[i] = Value();
	}

	inline Value& operator[](const Key& key) const { return *Find(key); }

	class ConstIterator
	{
	public:
		int currBucketIndex = 0;
		mutable int currIndex = 0;
		const StaticHashMap* hashMap;
	public:
		ConstIterator(const StaticHashMap* map, int cBucketIndex, int cIndex)
			: hashMap(map), currBucketIndex(cBucketIndex), currIndex(cIndex) { }

		const Value& operator*() const { return hashMap->arr[currBucketIndex * BucketSize + currIndex]; }

		// prefix increment
		const ConstIterator& operator++() const {
			currBucketIndex += ++currIndex >= hashMap->numElements[currBucketIndex];
			currIndex *= currIndex < hashMap->numElements[currBucketIndex];
			return *this;
		}

		// postfix increment
		const ConstIterator operator++(int amount) const {
			currBucketIndex += ++currIndex >= hashMap->numElements[currBucketIndex];
			currIndex *= currIndex < hashMap->numElements[currBucketIndex];
			return *this;
		}

		const Value* operator->() const { return &hashMap->arr[currBucketIndex * BucketSize + currIndex]; }

		bool operator == (const ConstIterator& other) const {
			return currBucketIndex == other.currBucketIndex && currIndex == other.currIndex;
		}
		bool operator != (const ConstIterator& other) const {
			return currBucketIndex != other.currBucketIndex || currIndex != other.currIndex;
		}
		bool operator < (const ConstIterator& other) const {
			return currBucketIndex * BucketSize + currIndex < other.currBucketIndex* BucketSize + other.currIndex;
		}
		bool operator > (const ConstIterator& other) const {
			return currBucketIndex * BucketSize + currIndex > other.currBucketIndex * BucketSize + other.currIndex;
		}
	};

	ConstIterator cbegin() const { return Iterator(this, 0, 0); }
	ConstIterator cend()   const { return Iterator(this, Size - 1, BucketSize - 1); }

};

template<typename Key, typename Value, int Size = 61, char BucketSize = 4>
class StaticSet : public StaticHashMap<Key, Value, Size, BucketSize>
{

};

