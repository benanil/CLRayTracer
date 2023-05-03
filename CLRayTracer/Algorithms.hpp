#pragma once

template<typename T, typename PT = int>
struct PriorityVal
{
	PT priority; T val;
	PriorityVal() {}
	PriorityVal(PT prior, T v) : priority(prior), val(v) {}
	bool operator== (const PriorityVal& o) const { return priority == o.priority; }
	bool operator!= (const PriorityVal& o) const { return priority != o.priority; }
	bool operator< (const PriorityVal& o) const { return priority < o.priority; }
	bool operator> (const PriorityVal& o) const { return priority > o.priority; }
};

inline bool IsNumber(char a) { return a <= '9' && a >= '0'; };
inline bool IsLower(char a) { return a >= 'a' && a <= 'z'; };
inline bool IsUpper(char a) { return a >= 'A' && a <= 'Z'; };
inline bool ToLower(char a) { return a < 'a' ? a + ('A' - 'a') : a; }
inline bool ToUpper(char a) { return a > 'Z' ? a - 'a' + 'A' : a; }
inline bool IsChar(char a) { return IsUpper(a) || IsLower(a); };
inline bool IsWhitespace(char c) { return (c == ' ' || c == '\t' || c == '\r'); }
template<typename T> inline void Swap(T& a, T& b) { T t = a; a = b, b = t; }

template<typename T>
inline void Swap(T& a, T& b)
{
	T temp = (T&&)a;
	a = (T&&)b;
	b = (T&&)temp;
}

template<typename T>
inline void BubbleSort(T* arr, int len)
{
    for (int i = 0; i < len-1; ++i)
    {
        for (int j = 0; j < len-i-1; ++j)
        {
            if (arr[j] > arr[j + 1])
                Swap(arr[j], arr[j + 1]);
        }
    }
}

// smaller stack size compared to quicksort
template<typename T>
inline void ShellSort(T* arr, int n)
{
	for (int gap = n / 2; gap > 0; gap /= 2)
	{
		for (int i = gap; i < n; ++i)
		{
			T temp = arr[i];
			int j = i;
			for (; j >= gap && arr[j - gap] > temp; j -= gap)
				arr[j] = arr[j - gap];

			arr[j] = temp;
		}
	}
}

template<typename T>
inline void QuickSort(T* arr, int left, int right)
{
	int i, j;
	while (right > left)
	{
		j = right;
		i = left - 1;
		T v = arr[right];

		while (true)
		{
			do i++; while (arr[i] < v && i < j);
			do j--; while (arr[j] > v && i < j);

			if (i >= j) break;
			Swap(arr[i], arr[j]);
		}

		Swap(arr[i], arr[right]);

		if ((i - 1 - left) <= (right - i - 1))
		{
			QuickSort(arr, left, i - 1);
			left = i + 1;
		}
		else
		{
			QuickSort(arr, i + 1, right);
			right = i - 1;
		}
	}
}

template<typename T>
inline void Reverse(T* begin, T* end)
{
	while (begin < end)
	{
		Swap(*begin, *end);
		begin++;
		end--;
	}
}

template<typename T>
inline T* BinarySearch(T* begin, int len, T value)
{
	int low = 0;
	int high = len;

	while (low < high)
	{
		T mid = (low + high) / 2;
		if (begin[mid] == value) return begin + mid;
		else if (begin[mid] < value) low = mid + 1;
		else high = mid - 1; // begin[mid] > value
	}
	return nullptr;
}

template<typename T>
inline T ParseNumber(const char*& curr)
{
	while (*curr && (*curr != '-' && !IsNumber(*curr))) curr++; // skip whitespace
	T val = 0;
	bool negative = false;
	if (*curr == '-') curr++, negative = true;
	while (*curr > '\n' && IsNumber(*curr))
		val = val * 10 + (*curr++ - '0');
	if (negative) val = -val;
	return val;
}

template<typename T>
inline void ParseNumberRef(const char*& curr, T& val) {
	val = ParseNumber<T>(curr);
}

template<typename T>
inline bool TryParse(T& val, const char*& curr)
{   // additional checks
	if (*curr == 0 || *curr == '\n') return false;
	while (IsWhitespace(*curr)) curr++;
	if (!IsNumber(*curr) || *curr != '-') return false;
	val = ParseNumber<T>(curr);
	return true;
}

inline bool StartsWith(const char*& curr, const char* str)
{
	const char* currStart = curr;
	while (IsWhitespace(*curr)) curr++;
	if (*curr != *str) return false;
	while (*str && *curr++ == *str++);
	bool isEqual = *str == 0;
	if (!isEqual) curr = currStart;
	return isEqual;
}

template<typename T> 
inline void Fill(T* begin, T* end, const T& val)
{
	while (begin < end) *begin++ = val;
}

template<typename T> 
inline void FillN(T* arr, T val, int n)
{
	for (int i = 0; i < n; ++i) arr[i] = val;
}

template<typename T> 
inline bool Contains(const T* arr, const T& val, int n)
{
    for (int i = 0; i < n; ++i) 
        if (arr[i] == val) return true;
    return false;
}

template<typename T> 
inline int IndexOf(const T* arr, const T& val, int n)
{
    for (int i = 0; i < n; ++i) 
        if (arr[i] == val) return i;
    return -1;
}

template<typename T> 
inline int CountIf(const T* arr, const T& val, int n)
{
    int count = 0;
    for (int i = 0; i < n; ++i) 
        count += arr[i] == val;
    return count;
}
