#pragma once

#include <cstdint>
#include <algorithm>

//only basic types allowed currently
template <typename storage>
class DictTree {
	struct stored {
		//store the stored item size in binary
		constexpr uint32_t size() {
			return sizeof(storage);
		};
		virtual uint32_t save(char* buffer, uint32_t max_buff = 0xFFFFFFFF){
			if(max_buff<size()) return 0;
			*((storage*)buffer)=content;
			return size();
		};
		storage content;
		stored(const storage& src):content(src){};
		stored(storage&& src):content(std::move(src)) {};
		stored& operator=(const storage& src) {content = src;};
		stored& operator=(storage&& src) { content = std::move(src); };
	};
	struct node {
		char* str;
		node* parent;
		node* left;
		node* right;
		uint32_t str_len;
		stored item;
		//takes all memory ownership
		node(const stored& i, char* string = nullptr, node* p = nullptr, node* lc = nullptr, node* rc = nullptr)
			:str(string), parent(p), left(lc), right(rc), item(i), str_len(strlen(str)) {};
		//takes all memory ownership
		node(stored&& i, char* string = nullptr, node* p = nullptr, node* lc = nullptr, node* rc = nullptr)
			:str(string), parent(p), left(lc), right(rc), item(std::move(i)), str_len(strlen(str)) {};
		// true if the two strings are same in prefix, but both have trailing bytes (2 new nodes)
		// if false, 
		// check if num==tot, if so current node is prefix of string (1 new node)
		// if num == tot + 1, current node is same as string (0 new node)
		// else (num < tot) current node has a suffix of string (1 new node)
		inline bool check_split(const char* const target, uint32_t& num, uint32_t& tot) {
			tot = str_len;
			uint32_t same=0;
			while(target[same] && same < str_len && target[same]==str[same]) ++same;
			if(!target[same]&&same==str_len) ++same;
			num=same;
			return target[same] && same < str_len;
		}
	};
};
