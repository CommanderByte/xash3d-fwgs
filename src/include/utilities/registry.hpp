#ifndef XASH_UTILITIES_REGISTRY_HPP
#define XASH_UTILITIES_REGISTRY_HPP

#include <stddef.h>
#include <string.h>

namespace xash
{
namespace utilities
{

enum class RegistryStatusCode
{
	Ok,
	Full,
	DuplicateKey,
};

struct RegistryStatus
{
	RegistryStatusCode code;
	const char *message;

	bool ok() const { return code == RegistryStatusCode::Ok; }
};

inline RegistryStatus MakeRegistryStatus(RegistryStatusCode code, const char *message)
{
	RegistryStatus status = { code, message };
	return status;
}

inline RegistryStatus RegistryOk()
{
	return MakeRegistryStatus(RegistryStatusCode::Ok, "");
}

enum class RegistryDuplicatePolicy
{
	Reject,
	Replace,
};

template <typename TKey>
struct DefaultKeyEqual
{
	bool operator()(const TKey &a, const TKey &b) const
	{
		return a == b;
	}
};

struct CStringKeyEqual
{
	bool operator()(const char *a, const char *b) const
	{
		if (!a || !b)
			return a == b;

		return strcmp(a, b) == 0;
	}
};

struct CaseInsensitiveCStringKeyEqual
{
	bool operator()(const char *a, const char *b) const
	{
		if (!a || !b)
			return a == b;

		while (*a && *b)
		{
			if (ToLower(*a) != ToLower(*b))
				return false;

			++a;
			++b;
		}

		return *a == *b;
	}

private:
	static char ToLower(char value)
	{
		if (value >= 'A' && value <= 'Z')
			return static_cast<char>(value - 'A' + 'a');

		return value;
	}
};

template <typename TKey, typename TEntry>
struct RegistryRecord
{
	TKey key;
	TEntry entry;
};

template <typename TKey, typename TEntry, size_t Capacity,
	typename TKeyEqual = DefaultKeyEqual<TKey> >
class StaticRegistry
{
public:
	typedef RegistryRecord<TKey, TEntry> Record;

	StaticRegistry()
		: m_count(0)
		, m_keyEqual(TKeyEqual())
	{
		static_assert(Capacity > 0, "StaticRegistry capacity must be greater than zero");
	}

	explicit StaticRegistry(const TKeyEqual &keyEqual)
		: m_count(0)
		, m_keyEqual(keyEqual)
	{
		static_assert(Capacity > 0, "StaticRegistry capacity must be greater than zero");
	}

	RegistryStatus registerEntry(const TKey &key, const TEntry &entry,
		RegistryDuplicatePolicy duplicatePolicy = RegistryDuplicatePolicy::Reject)
	{
		size_t index;

		if (findIndex(key, index))
		{
			if (duplicatePolicy == RegistryDuplicatePolicy::Reject)
				return MakeRegistryStatus(RegistryStatusCode::DuplicateKey, "registry key already exists");

			m_records[index].entry = entry;
			return RegistryOk();
		}

		if (m_count >= Capacity)
			return MakeRegistryStatus(RegistryStatusCode::Full, "registry is full");

		m_records[m_count].key = key;
		m_records[m_count].entry = entry;
		++m_count;
		return RegistryOk();
	}

	const TEntry *find(const TKey &key) const
	{
		size_t index;

		if (!findIndex(key, index))
			return NULL;

		return &m_records[index].entry;
	}

	TEntry *find(const TKey &key)
	{
		size_t index;

		if (!findIndex(key, index))
			return NULL;

		return &m_records[index].entry;
	}

	bool contains(const TKey &key) const
	{
		size_t index;
		return findIndex(key, index);
	}

	const Record *recordAt(size_t index) const
	{
		if (index >= m_count)
			return NULL;

		return &m_records[index];
	}

	Record *recordAt(size_t index)
	{
		if (index >= m_count)
			return NULL;

		return &m_records[index];
	}

	size_t count() const
	{
		return m_count;
	}

	size_t capacity() const
	{
		return Capacity;
	}

	bool empty() const
	{
		return m_count == 0;
	}

	bool full() const
	{
		return m_count == Capacity;
	}

	void clear()
	{
		m_count = 0;
	}

private:
	bool findIndex(const TKey &key, size_t &index) const
	{
		for (size_t i = 0; i < m_count; ++i)
		{
			if (m_keyEqual(m_records[i].key, key))
			{
				index = i;
				return true;
			}
		}

		return false;
	}

	Record m_records[Capacity];
	size_t m_count;
	TKeyEqual m_keyEqual;
};

}
}

#endif
