#include "oram.hpp"

#include "utility.hpp"

#include <boost/format.hpp>

namespace PathORAM
{
	using namespace std;
	using boost::format;

	ORAM::ORAM(ulong logCapacity, ulong blockSize, ulong Z, AbsStorageAdapter *storage, AbsPositionMapAdapter *map, AbsStashAdapter *stash) :
		storage(storage),
		map(map),
		stash(stash),
		dataSize(blockSize - sizeof(ulong)),
		Z(Z)
	{
		this->height  = logCapacity;			  // we are given a height
		this->buckets = (ulong)1 << this->height; // number of buckets is 2^height
		this->blocks  = this->buckets * this->Z;  // number of blocks is buckets / Z (Z blocks per bucket)

		// cout << logCapacity << endl;
		// cout << blockSize << endl;
		// cout << Z << endl;
		// cout << height << endl;
		// cout << buckets << endl;
		// cout << blocks << endl;

		// fill all blocks with random bits
		for (ulong i = 0uLL; i < this->buckets; i++)
		{
			for (ulong j = 0uLL; j < this->Z; j++)
			{
				auto block = getRandomBlock(this->dataSize);
				this->storage->set(i * this->Z + j, {ULONG_MAX, block});
			}
		}

		// generate random position map
		for (ulong i = 0; i < this->blocks; ++i)
		{
			this->map->set(i, getRandomULong(1 << (this->height - 1)));
		}
	}

	ORAM::~ORAM() {}

	bytes ORAM::get(ulong block)
	{
		return this->access(true, block, bytes());
	}

	void ORAM::put(ulong block, bytes data)
	{
		this->access(false, block, data);
	}

	bytes ORAM::access(bool read, ulong block, bytes data)
	{
		// step 1 from paper: remap block
		auto previousPosition = this->map->get(block);
		this->map->set(block, getRandomULong(1 << (this->height - 1)));

		// step 2 from paper: read path
		this->readPath(previousPosition); // stash updated

		// step 3 from paper: update block
		if (!read) // if "write"
		{
			this->stash->update(block, data);
		}
		auto returned = this->stash->get(block);

		// step 4 from paper: write path
		writePath(previousPosition); // stash updated

		return returned;
	}

	void ORAM::readPath(ulong leaf)
	{
		for (ulong level = 0; level < this->height; level++)
		{
			auto bucket = this->bucketForLevelLeaf(level, leaf);
			for (ulong i = 0; i < this->Z; i++)
			{
				auto block		= bucket * this->Z + i;
				auto [id, data] = this->storage->get(block);
				if (id != ULONG_MAX)
				{
					this->stash->add(id, data);
				}
			}
		}
	}

	void ORAM::writePath(ulong leaf)
	{
		auto currentStash = this->stash->getAll();
		vector<int> toDelete;

		for (int level = this->height - 1; level >= 0; level--)
		{
			vector<pair<ulong, bytes>> toInsert;
			vector<int> toDeleteLocal;
			for (auto entry : currentStash)
			{
				auto entryLeaf = this->map->get(entry.first);
				if (this->canInclude(entryLeaf, leaf, level))
				{
					toInsert.push_back(entry);
					toDelete.push_back(entry.first);
					toDeleteLocal.push_back(entry.first);
					if (toInsert.size() == this->Z)
					{
						break;
					}
				}
			}
			for (auto removed : toDeleteLocal)
			{
				currentStash.erase(removed);
			}

			auto bucket = this->bucketForLevelLeaf(level, leaf);

			for (ulong i = 0; i < this->Z; i++)
			{
				auto block = bucket * this->Z + i;
				if (toInsert.size() != 0)
				{
					auto data = toInsert.back();
					toInsert.pop_back();
					this->storage->set(block, data);
				}
				else
				{
					this->storage->set(block, {ULONG_MAX, getRandomBlock(this->dataSize)});
				}
			}
		}

		for (auto removed : toDelete)
		{
			this->stash->remove(removed);
		}
	}

	ulong ORAM::bucketForLevelLeaf(ulong level, ulong leaf)
	{
		return (leaf + (1 << (this->height - 1))) >> (height - 1 - level);
	}

	bool ORAM::canInclude(ulong pathLeaf, ulong blockPosition, ulong level)
	{
		return this->bucketForLevelLeaf(level, pathLeaf) == this->bucketForLevelLeaf(level, blockPosition);
	}

	void ORAM::checkConsistency()
	{
		for (ulong block = 0; block < this->blocks; block++)
		{
			// skip intital random blocks;
			// invariant does not apply to them;
			if (this->storage->get(block).first == ULONG_MAX)
			{
				continue;
			}

			// clean the stash
			auto oldStash = this->stash->getAll();
			for (auto entry : oldStash)
			{
				this->stash->remove(entry.first);
			}

			// get the leaf pointed by the position map
			auto leaf = this->map->get(block);

			// read the path to the stash
			this->readPath(leaf);

			// look for block in the stash
			for (auto entry : this->stash->getAll())
			{
				if (entry.first == block)
				{
					// found
					break;
				}
			}
			// not found
			throw boost::format("block %1% is mapped to leaf %2%, but was not found in the path") % block % leaf;
		}
	}
}
