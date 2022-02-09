#pragma once

class SimpleHeap
{
public:
	SimpleHeap(int capacity_count)
	{
		m_free[0] = capacity_count;
	}

	int alloc(int count, int align = 1)
	{
		// Find the first free fragment that is large enough.
		auto it = std::find_if(m_free.begin(), m_free.end(), [&](const auto& x) {
			auto start_offset = (align - (x.first % align)) % align;
			return (x.second - start_offset) >= count;
			});

		// Fail if there's no room.
		if (it == m_free.end())
			return -1;

		// Remove the block from the free list.
		auto free_start = it->first;
		auto free_count = it->second;
		m_free.erase(it);

		// Determine the allocation start offset, respecting alignment.
		auto alloc_offset = (align - (free_start % align)) % align;
		auto alloc_start = free_start + alloc_offset;
		m_used[alloc_start] = count;

		// Add any unused fragment before the allocation.
		if (free_start < alloc_start)
			m_free[free_start] = alloc_start - free_start;

		// Add any unused fragment after the allocation.
		if ((alloc_offset + count) < free_count)
			m_free[alloc_start + count] = free_count - (alloc_offset + count);

		// Return the start offset.
		return alloc_start;
	}

	void free(int start)
	{
		// Look up the start offset to determine the allocation size.
		auto it = m_used.find(start);
		if (it == m_used.end())
			throw std::exception("invalid heap free");

		// Return the allocation to the free list.
		m_free[start] = it->second;
		m_used.erase(it);

		// Scan the free list to combine neighbouring entries.
		for (it = m_free.begin(); it != m_free.end(); )
		{
			auto itNext = std::next(it);
			if (itNext != m_free.end())
			{
				// Does this entry finish where the next starts?
				if (it->first + it->second == itNext->first)
				{
					// If so, combine them and remove the redundant entry.
					it->second += itNext->second;
					m_free.erase(itNext);
					continue;
				}
			}

			++it;
		}
	}

protected:
	std::map<int, int> m_free;
	std::map<int, int> m_used;
};
