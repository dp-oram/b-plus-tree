#include "definitions.h"
#include "tree.hpp"
#include "utility.hpp"

#include "gtest/gtest.h"
#include <boost/algorithm/string.hpp>

using namespace std;

#define ASSERT_THROW_CONTAINS(statement, substring)                                                                     \
	try                                                                                                                 \
	{                                                                                                                   \
		statement;                                                                                                      \
		FAIL() << "statement did not throw exception";                                                                  \
	}                                                                                                                   \
	catch (Exception exception)                                                                                         \
	{                                                                                                                   \
		if (!boost::icontains(exception.what(), substring))                                                             \
		{                                                                                                               \
			FAIL() << "exception message does not contain '" << substring << "', the message is: " << exception.what(); \
		}                                                                                                               \
	}                                                                                                                   \
	catch (...)                                                                                                         \
	{                                                                                                                   \
		FAIL() << "statement threw unexpected exception";                                                               \
	}

namespace BPlusTree
{
	bytes generateDataBytes(string word, int size);
	vector<pair<number, bytes>> generateDataPoints(int from, int to, int size, int duplicates = 1);
	pair<number, vector<pair<number, number>>> generatePairs(number BLOCK_SIZE);

	class TreeTest : public testing::TestWithParam<number>
	{
		public:
		inline static number BLOCK_SIZE;

		protected:
		unique_ptr<Tree> tree;
		shared_ptr<AbsStorageAdapter> storage;

		TreeTest()
		{
			BLOCK_SIZE = GetParam();
			storage	   = make_shared<InMemoryStorageAdapter>(BLOCK_SIZE);
		}

		vector<pair<number, bytes>> populateTree(int from = 5, int to = 15, int size = 100, int duplicates = 1)
		{
			auto data = generateDataPoints(from, to, size, duplicates);
			tree	  = make_unique<Tree>(storage, data);

			return data;
		}
	};

	bytes generateDataBytes(string word, int size)
	{
		stringstream ss;
		for (int i = 0; i < size; i += word.size())
		{
			ss << word;
		}

		return fromText(ss.str(), size);
	}

	vector<pair<number, bytes>> generateDataPoints(int from, int to, int size, int duplicates)
	{
		vector<pair<number, bytes>> data;
		for (int i = from; i <= to; i++)
		{
			for (int j = 0; j < duplicates; j++)
			{
				data.push_back({i, generateDataBytes(to_string(i), size)});
			}
		}

		return data;
	}

	pair<number, vector<pair<number, number>>> generatePairs(number BLOCK_SIZE)
	{
		auto count = (BLOCK_SIZE - sizeof(number)) / sizeof(number) / 2;

		vector<pair<number, number>> pairs;
		pairs.resize(count);
		for (uint i = 0; i < count; i++)
		{
			pairs[i].first	= i;
			pairs[i].second = i * 1000;
		}

		return {count, pairs};
	}

	TEST_P(TreeTest, Initialization)
	{
		auto data = generateDataPoints(5, 7, 100);

		ASSERT_NO_THROW(make_unique<Tree>(storage, data));
	}

	TEST_P(TreeTest, BlockSizeTooSmall)
	{
		auto storage = make_shared<InMemoryStorageAdapter>(4 * sizeof(number));
		vector<pair<number, bytes>> empty;
		ASSERT_THROW_CONTAINS(make_unique<Tree>(storage, empty), "block size too small");
	}

	TEST_P(TreeTest, ReadDataLayer)
	{
		const auto from = 5;
		const auto to	= 7;
		const auto size = 100;

		auto data = generateDataPoints(from, to, size);

		tree = make_unique<Tree>(storage, data);

		auto current = tree->leftmostDataBlock;
		for (uint i = from; i <= to; i++)
		{
			auto [type, read] = tree->checkType(current);
			ASSERT_EQ(DataBlock, type);

			auto [payload, key, next] = tree->readDataBlock(read);
			ASSERT_EQ(size, payload.size());
			auto block = find_if(
				data.begin(),
				data.end(),
				[i](const pair<number, bytes>& val) {
					return val.first == i;
				});
			current = next;

			ASSERT_EQ((*block).second, payload);
			ASSERT_EQ(i, key);
		}
	}

	TEST_P(TreeTest, CreateNodeBlockTooBig)
	{
		tree = make_unique<Tree>(storage);
		vector<pair<number, number>> pairs;
		pairs.resize(BLOCK_SIZE / 2);

		ASSERT_ANY_THROW(tree->createNodeBlock(pairs));
	}

	TEST_P(TreeTest, CreateNodeBlock)
	{
		tree = make_unique<Tree>(storage);

		auto pairs = generatePairs(BLOCK_SIZE).second;

		ASSERT_NO_THROW(tree->createNodeBlock(pairs));
	}

	TEST_P(TreeTest, ReadNodeBlock)
	{
		tree = make_unique<Tree>(storage);

		auto pairs = generatePairs(BLOCK_SIZE).second;

		auto address	   = tree->createNodeBlock(pairs);
		auto [type, block] = tree->checkType(address);
		ASSERT_EQ(NodeBlock, type);
		auto read = tree->readNodeBlock(block);

		ASSERT_EQ(pairs, read);
	}

	TEST_P(TreeTest, ReadWrongNodeBlock)
	{
		tree = make_unique<Tree>(storage);

		auto pairs = generatePairs(BLOCK_SIZE).second;

		auto address = tree->createNodeBlock(pairs);
		auto block	 = tree->checkType(address).second;

		ASSERT_THROW_CONTAINS(tree->readDataBlock(block), "non-data block");
	}

	TEST_P(TreeTest, ReadWrongDataBlock)
	{
		populateTree();

		auto address = tree->leftmostDataBlock;
		auto block	 = tree->checkType(address).second;

		ASSERT_THROW_CONTAINS(tree->readNodeBlock(block), "non-node block");
	}

	TEST_P(TreeTest, PushLayer)
	{
		tree = make_unique<Tree>(storage);

		auto [count, pairs] = generatePairs(BLOCK_SIZE * 2);

		auto pushed	 = tree->pushLayer(pairs);
		auto counter = 0;
		for (uint i = 0; i < pushed.size(); i++)
		{
			auto [type, read] = tree->checkType(pushed[i].second);
			ASSERT_EQ(NodeBlock, type);

			auto block = tree->readNodeBlock(read);
			for (auto key : block)
			{
				EXPECT_LE(key.first, pushed[i].first);
				EXPECT_EQ(key.first * 1000, key.second);
				counter++;
			}
		}
		EXPECT_EQ(count, counter);
	}

	TEST_P(TreeTest, BasicSearch)
	{
		const auto query = 10uLL;

		auto data = populateTree();

		vector<bytes> returned;
		tree->search(query, returned);
		auto expected = find_if(
			data.begin(),
			data.end(),
			[](const pair<number, bytes>& val) {
				return val.first == query;
			});

		ASSERT_NE(0, returned.size());
		ASSERT_EQ((*expected).second, returned[0]);
	}

	TEST_P(TreeTest, BasicSearchNotFound)
	{
		const auto query = 20uLL;

		auto data = populateTree();

		vector<bytes> returned;
		tree->search(query, returned);

		ASSERT_EQ(0, returned.size());
	}

	TEST_P(TreeTest, BasicSearchDuplicates)
	{
		const auto query	  = 10uLL;
		const auto duplicates = 3;

		auto data = populateTree(5, 15, 100, duplicates);

		vector<bytes> returned;
		tree->search(query, returned);

		vector<bytes> expected;
		auto i = data.begin();
		while (i != data.end())
		{
			i = find_if(
				i,
				data.end(),
				[](const pair<number, bytes>& val) {
					return val.first == query;
				});
			if (i != data.end())
			{
				auto element = (*i).second;
				expected.push_back(element);
				i++;
			}
		}

		ASSERT_EQ(duplicates, returned.size());
		ASSERT_EQ(expected, returned);
	}

	TEST_P(TreeTest, BasicSearchRangeDuplicates)
	{
		const auto start	  = 8uLL;
		const auto end		  = 11uLL;
		const auto duplicates = 3;

		auto data = populateTree(5, 15, 100, duplicates);

		vector<bytes> returned;
		tree->search(start, end, returned);

		vector<bytes> expected;
		auto i = data.begin();
		while (i != data.end())
		{
			i = find_if(
				i,
				data.end(),
				[](const pair<number, bytes>& val) {
					return val.first >= start && val.first <= end;
				});
			if (i != data.end())
			{
				auto element = (*i).second;
				expected.push_back(element);
				i++;
			}
		}
		ASSERT_EQ(expected, returned);
	}

	TEST_P(TreeTest, BasicSearchAll)
	{
		const auto start = 5uLL;
		const auto end	 = 15uLL;

		auto data = populateTree();

		vector<bytes> returned;
		tree->search(start, end, returned);

		vector<bytes> expected;
		expected.resize(data.size());

		transform(
			data.begin(),
			data.end(),
			expected.begin(),
			[](pair<number, bytes> val) { return val.second; });

		ASSERT_EQ(expected, returned);
	}

	TEST_P(TreeTest, SearchAllDisaster)
	{
		const auto start	 = 5uLL;
		const auto end		 = 15uLL;
		const auto FILE_NAME = "tree.bin";

		auto data = generateDataPoints(start, end, 100, 1);
		storage	  = make_shared<FileSystemStorageAdapter>(BLOCK_SIZE, FILE_NAME, true);
		tree	  = make_unique<Tree>(storage, data);

		tree.reset();

		storage = make_shared<FileSystemStorageAdapter>(BLOCK_SIZE, FILE_NAME, false);
		tree	= make_unique<Tree>(storage);

		vector<bytes> returned;
		tree->search(start, end, returned);

		vector<bytes> expected;
		expected.resize(data.size());

		transform(
			data.begin(),
			data.end(),
			expected.begin(),
			[](pair<number, bytes> val) { return val.second; });

		ASSERT_EQ(expected, returned);

		remove(FILE_NAME);
	}

	TEST_P(TreeTest, ConsistencyCheck)
	{
		populateTree();

		ASSERT_NO_THROW(tree->checkConsistency());
	}

	TEST_P(TreeTest, ConsistencyCheckWrongBlockType)
	{
		populateTree();

		bytes root;
		storage->get(tree->root, root);
		root[0] = 0xff;
		storage->set(tree->root, root);

		ASSERT_THROW_CONTAINS(tree->checkConsistency(), "block type");
	}

	TEST_P(TreeTest, ConsistencyCheckDataBlockPointer)
	{
		populateTree();

		bytes dataBlock;
		storage->get(tree->leftmostDataBlock, dataBlock);
		dataBlock[sizeof(number) * 2] = storage->empty();
		storage->set(tree->leftmostDataBlock, dataBlock);

		ASSERT_THROW_CONTAINS(tree->checkConsistency(), "data block");
	}

	TEST_P(TreeTest, ConsistencyCheckDataBlockKey)
	{
		populateTree();

		bytes dataBlock;
		storage->get(tree->leftmostDataBlock, dataBlock);
		dataBlock[sizeof(number) * 3] = 0uLL;
		storage->set(tree->leftmostDataBlock, dataBlock);

		ASSERT_THROW_CONTAINS(tree->checkConsistency(), "key");
	}

	string printTestName(testing::TestParamInfo<number> input)
	{
		return to_string(input.param);
	}

	number cases[] = {
		64,
		128,
		256,
	};

	INSTANTIATE_TEST_SUITE_P(TreeTestSuite, TreeTest, testing::ValuesIn(cases), printTestName);
}

int main(int argc, char** argv)
{
	srand(TEST_SEED);

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
