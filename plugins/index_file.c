/*
 * index_file.cpp
 *
 *  Created on: Oct 26, 2008
 *      Author: monfee
 */

#include "index_file.h"
#include "word.h"
#include "address.h"

#include <cassert>
#include <iostream>

using namespace std;

IndexFile::~IndexFile() {

}

void IndexFile::init() {
	EXT_NAME = "idx";
	wlen_ = 0;
	File::setup();
}

unsigned int IndexFile::cal(array_type& arr) {
	return 0;
}

void IndexFile::alloc(array_type& arr) {
	if (arr.size() < 1)
		return;

	Address::uint_array forcopy(ADDRESSES_NUMBER, Address::INVALID_BOUND);
	Address::uint_array bound(ADDRESSES_NUMBER, Address::INVALID_BOUND);

	word_ptr_type word_ptr = arr[0];
	int ffs = (int)word_ptr->family().first->array().size();
	cout << "ffs: " << ffs << endl;
	assert( ffs == (wlen_ - 1));
	//string_type pre_parent = word_ptr->family().first->chars();
	Word::Side side = word_ptr->side(); //LEFT;
	assert(side != Word::UNKNOWN);
	if (side == Word::LEFT)
		bound[0] = 0;
	else if (side == Word::RIGHT)
		bound[1] = 0;

	bound[2] = 0;
	int change = 0;

	int i = 0;
	for (i = 1; i < (int)arr.size(); i++) {
		//word_ptr = arr[i];
		//string_type p_str = arr[i]->family().first->chars();

		assert(arr[i]->side() != Word::UNKNOWN);

		if (word_ptr->family().first->chars() != arr[i]->family().first->chars()) {
			bound[2] = i - 1;

			aa_.push_back(Address(Word::array_to_array(word_ptr->family().first->array()), bound));
			bound.clear();
			bound = forcopy;

			word_ptr = arr[i];
			//assert(side != Word::LEFT);
			assert(change < 2);
			side = word_ptr->side();//arr[i]->side();

			bound[2] = i;
			if (side == Word::LEFT)
				bound[0] = i;
			else if (side == Word::RIGHT)
				bound[1] = i;
			//bound[0] = i;

			//pre_parent = p_str;
			change = 0;
		} else {
			if (side != arr[i]->side()) {
				bound[1] = i - 1;
				//bound[1] = i - 1;
				side = Word::RIGHT;
				change++;
			}
		}
	} // for

	bound[2] = i - 1;
	assert(change < 2);
	aa_.push_back(Address(Word::array_to_array(word_ptr->family().first->array()), bound));
}

void IndexFile::write() {

	File::wopen();
	char* char_ptr = NULL;
	for (int i = 0; i < (int)aa_.size(); i++) {

		//cout << "index chars size: " << (int)aa_[i].ca().size() << endl;
		assert((int)aa_[i].ca().size() == (wlen_ - 1));
		for (int j = 0; j < (int)aa_[i].ca().size(); j++) {
			char_ptr = (char*)aa_[i].ca()[j].c_str();
			iofs_.write(char_ptr, UNICODE_CHAR_LENGTH);
		}

		for (int j = 0; j < (int)ADDRESSES_NUMBER; j++) {
			int bound = aa_[i].bound()[j];
			iofs_.write((char*)&bound, UINT_SIZE);
		}

	}
	iofs_.close();
}

void IndexFile::read() {
	assert(wlen_ > 0);

	if (wlen_ == 1)
		return;

	if (!iofs_.is_open())
		File::ropen();

	int chars_len = wlen_ - 1;

	int record_length = UNICODE_CHAR_LENGTH * chars_len + ADDRESSES_LENGTH;

	char buf[UNICODE_CHAR_LENGTH];
	unsigned int address = 0;
	int count = 0;

	cout << "loading index file" << filename_ << endl;
	while (count < size_) {
		string_array ca;
		Address::uint_array addr(ADDRESSES_NUMBER, Address::INVALID_BOUND);

		for (int i = 0; i < chars_len; i++) {
			if (!iofs_.read (buf, UNICODE_CHAR_LENGTH)) {
				// Same effect as above
				cerr << "Errors when reading file \"" << name_ << "\"" << endl;
			}
			/// save them in the array
			string_type a_char =
				get_first_utf8char((unsigned char*)&buf);
			ca.push_back(a_char);
		}

		for (int i = 0; i < ADDRESSES_NUMBER; i++) {
			if (!iofs_.read ((char *)&address, sizeof(unsigned int))) {
				// Same effect as above
				cerr << "Errors when reading file \"" << name_ << "\"" << endl;
			}

			addr[i] = address;
		}

		aa_.push_back(Address(ca, addr));
		count += record_length;
	}
	iofs_.close();
}

