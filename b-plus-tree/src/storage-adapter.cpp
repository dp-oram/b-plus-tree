#include "storage-adapter.hpp"

#include "utility.hpp"

#include <boost/format.hpp>
#include <cstring>

namespace BPlusTree
{
	using namespace std;
	using boost::format;

#pragma region AbsStorageAdapter

	AbsStorageAdapter::~AbsStorageAdapter()
	{
	}

	AbsStorageAdapter::AbsStorageAdapter(number blockSize) :
		blockSize(blockSize)
	{
	}

	number AbsStorageAdapter::getBlockSize()
	{
		return blockSize;
	}

#pragma endregion AbsStorageAdapter

#pragma region InMemoryStorageAdapter

	InMemoryStorageAdapter::InMemoryStorageAdapter(number blockSize) :
		AbsStorageAdapter(blockSize)
	{
		auto emptyBlock = bytesFromNumber(empty());
		emptyBlock.resize(blockSize);
		set(meta(), emptyBlock);
	}

	InMemoryStorageAdapter::~InMemoryStorageAdapter()
	{
	}

	void InMemoryStorageAdapter::get(number location, bytes &response)
	{
		checkLocation(location);

		response.insert(response.begin(), memory[location].begin(), memory[location].end());
	}

	void InMemoryStorageAdapter::set(number location, const bytes &data)
	{
		if (data.size() != blockSize)
		{
			throw Exception(boost::format("data size (%1%) does not match block size (%2%)") % data.size() % blockSize);
		}

		checkLocation(location);

		memory[location] = data;
	}

	number InMemoryStorageAdapter::malloc()
	{
		return locationCounter++;
	}

	number InMemoryStorageAdapter::empty()
	{
		return EMPTY;
	}

	number InMemoryStorageAdapter::meta()
	{
		return META;
	}

	void InMemoryStorageAdapter::checkLocation(number location)
	{
		if (location >= locationCounter)
		{
			throw Exception(boost::format("attempt to access memory that was not malloced (%1%)") % location);
		}
	}

#pragma endregion InMemoryStorageAdapter

#pragma region FileSystemStorageAdapter

	FileSystemStorageAdapter::FileSystemStorageAdapter(number blockSize, string filename, bool override) :
		AbsStorageAdapter(blockSize)
	{
		auto flags = fstream::in | fstream::out | fstream::binary | fstream::ate;
		if (override)
		{
			flags |= fstream::trunc;
		}

		file.open(filename, flags);
		if (!file)
		{
			throw Exception(boost::format("cannot open %1%: %2%") % filename % strerror(errno));
		}

		locationCounter = override ? (2 * blockSize) : (number)file.tellg();

		if (override)
		{
			auto emptyBlock = bytesFromNumber(empty());
			emptyBlock.resize(blockSize);
			set(meta(), emptyBlock);
		}
	}

	FileSystemStorageAdapter::~FileSystemStorageAdapter()
	{
		file.close();
	}

	void FileSystemStorageAdapter::get(number location, bytes &response)
	{
		checkLocation(location);

		uchar placeholder[blockSize];
		file.seekg(location, file.beg);
		file.read((char *)placeholder, blockSize);

		response.insert(response.begin(), placeholder, placeholder + blockSize);
	}

	void FileSystemStorageAdapter::set(number location, const bytes &data)
	{
		if (data.size() != blockSize)
		{
			throw Exception(boost::format("data size (%1%) does not match block size (%2%)") % data.size() % blockSize);
		}

		checkLocation(location);

		uchar placeholder[blockSize];
		copy(data.begin(), data.end(), placeholder);

		file.seekp(location, file.beg);
		file.write((const char *)placeholder, blockSize);
	}

	number FileSystemStorageAdapter::malloc()
	{
		return locationCounter += blockSize;
	}

	number FileSystemStorageAdapter::empty()
	{
		return EMPTY;
	}

	number FileSystemStorageAdapter::meta()
	{
		return blockSize;
	}

	void FileSystemStorageAdapter::checkLocation(number location)
	{
		if (location > locationCounter || location % blockSize != 0)
		{
			throw Exception(boost::format("attempt to access memory that was not malloced (%1%)") % location);
		}
	}

#pragma endregion FileSystemStorageAdapter

}
