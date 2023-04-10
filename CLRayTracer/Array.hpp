#pragma once
#include "Common.hpp"

template<typename T>
struct ArrayIteratorBase
{
	T* Ptr;
};

template<typename T>
struct ArrayIterator : ArrayIteratorBase<T>
{
	T& operator*() const noexcept
	{
		return *this->Ptr;
	}

	T* operator->() const noexcept
	{
		return this->Ptr;
	}

	ArrayIterator& operator++()
	{
		this->Ptr++;
		return *this;
	}

	ArrayIterator operator++(int)
	{
		ArrayIterator tmp = *this;
		++(*this);
		return tmp;
	}

	bool operator==(const ArrayIterator<T>& other) const
	{
		return this->Ptr == other.Ptr;
	}

	bool operator!=(const ArrayIterator<T>& other) const
	{
		return this->Ptr != other.Ptr;
	}

	ArrayIterator operator+(uint index) const
	{
		return {this->Ptr + index};
	}

	ArrayIterator& operator+=(uint index) const
	{
		this->Ptr += index;
		return *this;
	}
};

template<typename T>
struct ArrayConstIterator : ArrayIteratorBase<T>
{
	T& operator*() const noexcept
	{
		return *this->Ptr;
	}

	T& operator->() const noexcept
	{
		return *this->Ptr;
	}

	const ArrayConstIterator& operator++()
	{
		this->Ptr++;
		return *this;
	}

	ArrayConstIterator operator++(int)
	{
		ArrayConstIterator tmp = *this;
		++(*this);
		return tmp;
	}

	bool operator==(const ArrayConstIterator<T>& other) const
	{
		return this->Ptr == other.Ptr;
	}

	bool operator!=(const ArrayConstIterator<T>& other) const
	{
		return this->Ptr != other.Ptr;
	}

	ArrayConstIterator operator+(uint index) const
	{
		return {this->Ptr + index};
	}

	ArrayConstIterator& operator+=(uint index) const
	{
		this->Ptr += index;
		return *this;
	}
};

template<typename T>
class Array
{
private:
	using size_type = uint;

	T* m_Data = nullptr;
	size_type m_Size = 0;
	size_type m_Capacity = 0;
public:
	using iterator_base = ArrayIteratorBase<T>;
	using iterator = ArrayIterator<T>;
	using const_iterator = ArrayConstIterator<T>;

	explicit Array(size_type defaultSize = 12)
	: m_Data(static_cast<T*>(calloc(1, sizeof(T) * defaultSize))),
	  m_Size(0), m_Capacity(defaultSize)
	{}

	~Array() { if (m_Data != nullptr) free(m_Data); }

	Array(const Array& other) : m_Data(), m_Size(other.m_Size), m_Capacity(other.m_Capacity)
	{
		m_Data = static_cast<T*>(calloc(1, sizeof(T) * other.m_Capacity));

		memcpy(m_Data, other.m_Data, other.DataSize());
	}

	Array(Array&& other) noexcept : m_Data(other.m_Data), m_Size(other.m_Size), m_Capacity(other.m_Capacity)
	{
		m_Data = other.m_Data;
		other.m_Data = nullptr;
		other.m_Size = 0;
		other.m_Capacity = 0;
	}

	Array& operator=(const Array& other)
	{
		m_Capacity = other.m_Capacity;
		m_Size = other.m_Size;

		if (m_Data != nullptr)
		{
			m_Data = static_cast<T*>(realloc(m_Data, other.m_Capacity * sizeof(T)));
		}
		else
		{
			m_Data = static_cast<T*>(calloc(1, other.m_Capacity * sizeof(T)));
		}

		memcpy(m_Data, other.m_Data, other.DataSize());
		return *this;
	}

	FINLINE iterator begin() { return iterator{m_Data}; }
	FINLINE iterator end()   { return iterator{m_Data + m_Size}; }
	FINLINE const_iterator begin() const { return const_iterator{m_Data}; }
	FINLINE const_iterator end() const   { return const_iterator{m_Data + m_Size}; }

	[[nodiscard]] FINLINE bool Any() const { return m_Size > 0; }
	[[nodiscard]] FINLINE bool Empty() const { return m_Size == 0; }

	FINLINE T& operator[](size_type index) { return m_Data[index]; }

	void Clear(int capacity = 12)
	{
		if (m_Size == 0) return;

		m_Size = 0;
		m_Capacity = capacity;
		m_Data = static_cast<T*>(realloc(m_Data, sizeof(T) * m_Capacity));
		// TODO: Should we do memset 0 here ? answer: yes.
		memset(m_Data, 0, sizeof(T) * m_Size);
	}

	T& Add(T type) 
	{
		if (m_Size + 1 > m_Capacity)
		{
			m_Capacity = CalculateGrowth(m_Size + 1);
			m_Data = static_cast<T*>(realloc(m_Data, sizeof(T) * m_Capacity));
		}

		return (m_Data[m_Size++] = type);
	}

	void AddRange(T* begin, T* end)
	{
		ulong len = Distance(begin, end);

		if (m_Size + len - 1 > m_Capacity)
		{
			m_Capacity = CalculateGrowth(m_Size + len);
			m_Data = static_cast<T*>(realloc(m_Data, sizeof(T) * m_Capacity));
		}

		while (begin != end) 
		{
			m_Data[m_Size++] = *begin++;
		}
	}

	void SetRange(size_type start, size_type end, T value) 
	{
		for (; start < end; ++start) m_Data[start] = value;
	}
	template<typename... Args>
	void EmplaceConstruct(Args&&... args)
	{
		if (m_Size + 1 > m_Capacity) {
			m_Capacity = CalculateGrowth(m_Size + 1);
			m_Data = static_cast<T*>(realloc(m_Data, sizeof(T) * m_Capacity));
		}
		m_Data[m_Size++] = T(args...); // forward here
	}

	T& Emplace(T&& type)
	{
		if (m_Size + 1 > m_Capacity)
		{
			m_Capacity = CalculateGrowth(m_Size + 1);
			m_Data = static_cast<T*>(realloc(m_Data, sizeof(T) * m_Capacity));
		}

		return (m_Data[m_Size++] = (T&&)(type));
	}

	void InsertAt(size_type  index, T type)
	{
		ax_assert(index > m_Size);

		if (index > m_Size)
			return;

		if (m_Size + 1 > m_Capacity)
		{
			m_Capacity = CalculateGrowth(m_Size + 1);
			m_Data = static_cast<T*>(realloc(m_Data, sizeof(T) * m_Capacity));
		}

		memmove(m_Data + index + 1, m_Data + index, (m_Size - index) * sizeof(T));
		m_Size++;

		m_Data[index] = type;
	}

	void InsertAtUnordered(size_type index, T type)
	{
		ax_assert(index > m_Size);

		if (m_Size + 1 > m_Capacity)
		{
			m_Capacity = CalculateGrowth(m_Size + 1);
			m_Data = static_cast<T*>(realloc(m_Data, sizeof(T) * m_Capacity));
		}

		m_Data[m_Size++] = m_Data[index];
		m_Data[index] = type;
	}

	void EmplaceAt(size_type index, T&& type)
	{
		ax_assert(index > m_Size);

		if (index > m_Size)
			return;

		if (m_Size + 1 > m_Capacity)
		{
			m_Capacity = CalculateGrowth(m_Size + 1);
			m_Data = static_cast<T*>(realloc(m_Data, sizeof(T) * m_Capacity));
		}

		memmove(m_Data + index + 1, m_Data + index, (m_Size - index) * sizeof(T));
		m_Size++;

		m_Data[index] = (T&&)(type);
	}

	void Insert(const Array& other)
	{
		if (other.m_Size > m_Capacity)
		{
			m_Capacity += other.m_Capacity;
			m_Data = static_cast<T*>(realloc(m_Data, sizeof(T) * m_Capacity));
		}

		memcpy(m_Data + m_Size, other.m_Data, other.DataSize());

		m_Size += other.m_Size;
	}

	// <summary> returns number of elements removed </summary>
	template<typename Func_t>
	int RemoveAll(Func_t match)
	{
		int freeIndex = 0;   // the first free slot in items array

		// Find the first item which needs to be removed.
		while (freeIndex < m_Size && match(m_Data[freeIndex]) == false) freeIndex++;
		if (freeIndex >= m_Size) return 0;

		int current = freeIndex + 1;
		while (current < m_Size)
		{
			// Find the first item which needs to be kept.
			while (current < m_Size && match(m_Data[current]) == true) current++;

			if (current < m_Size)
			{
				// copy item to the free slot.
				m_Data[freeIndex++] = m_Data[current++];
			}
		}

		int numCleared = m_Size - freeIndex;
		memset(m_Data + freeIndex, 0, numCleared * sizeof(T));
		m_Size = freeIndex;

		return numCleared; // removed item count
	}

	// faster but unordered, returns true if succesfuly removed(value is exist)
	bool RemoveUnordered(T value)
	{ 
		for (int i = 0; i < m_Size; ++i)
		{
			if (value == m_Data[i])
			{
				// put last element to removed place
				m_Data[i] = m_Data[--m_Size];
				memset(m_Data + m_Size, 0, sizeof(T)); // memset0 for old element
				return true;
			}
		}
		return false;
	}

	bool RemoveAtUnordered(uint32_t index)
	{
		if (index >= m_Size) return false;
		// put last element to removed place
		m_Data[index] = m_Data[--m_Size];
		memset(m_Data + m_Size, 0, sizeof(T));// memset0 for old element
		return true;
	}

	bool RemoveAt(uint32_t index)
	{
		if (index >= m_Size) return false;

		memmove(m_Data + index, m_Data + index + 1, (m_Size - index - 1) * sizeof(T));
		m_Size--;

		return true;
	}

	void Remove(T value)
	{
		for (uint32_t i = 0; i < m_Size; i++)
		{
			if (m_Data[i] == value)
			{
				memmove(m_Data + i, m_Data + i + 1, (m_Size - i - 1) * sizeof(T));
				m_Size--;
				return;
			}
		}
	}

	void Remove(bool(* match)(const T&))
	{
		for (uint32_t i = 0; i < m_Size; i++)
		{	
			if (match(m_Data[i]) == true)
			{
				memmove(m_Data + i, m_Data + i + 1, (m_Size - i - 1) * sizeof(T));
				m_Size--;
				return;
			}
		}
	}

	void Remove(const iterator_base& it)
	{
		int32_t index = it.Ptr - m_Data;

		ax_assert(index >= 0 && index < m_Size);

		if (index >= 0 && index < m_Size)
		{
			RemoveAt(uint32_t(index));
		}
	}

	T& At(size_type index)
	{
		return m_Data[index];
	}

	void Resize(ulong size)
	{
		m_Capacity = size;
		m_Data = static_cast<T*>(realloc(m_Data, sizeof(T) * m_Capacity));
	}

	size_type AddUninitialized(size_type count = 1)
	{
		ax_assert(count > 0);

		const size_type oldNum = Size();

		if ((m_Size += count) > m_Capacity)
		{
			m_Capacity += m_Size - m_Capacity;
			m_Data = static_cast<T*>(realloc(m_Data, sizeof(T) * m_Capacity));
		}

		return oldNum;
	}

	[[nodiscard]] FINLINE size_type Size() const { return m_Size; }
	[[nodiscard]] FINLINE size_type DataSize() const { return m_Size * sizeof(T); }
	[[nodiscard]] FINLINE size_type Capacity() const { return m_Capacity; }

	Array& operator+=(T type)
	{
		Add(type);
		return *this;
	}

	Array& operator-=(T type)
	{
		Remove(type);
		return *this;
	}
public:
	
	static void SetRange(T* start, T* end, T value) {
		for (; start < end; ++start) *start = value;
	}

private:
	FINLINE size_type CalculateGrowth(size_type newSize) const
	{
		return newSize + (newSize/2); // geometric growth is sufficient
	}
};

