#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>

#include <cjson/cJSON.h>

#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/base64.h>
#include <cryptopp/filters.h>

#define TAGLIB_STATIC
#include <taglib/tag.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/mpegfile.h>
#include <taglib/attachedpictureframe.h>

constexpr char mPng[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
constexpr CryptoPP::byte core_key[CryptoPP::AES::DEFAULT_KEYLENGTH] = { 0x68, 0x7A, 0x48, 0x52, 0x41, 0x6D, 0x73, 0x6F, 0x35, 0x6B, 0x49, 0x6E, 0x62, 0x61, 0x78, 0x57 };
constexpr CryptoPP::byte meta_key[CryptoPP::AES::DEFAULT_KEYLENGTH] = { 0x23, 0x31, 0x34, 0x6C, 0x6A, 0x6B, 0x5F, 0x21, 0x5C, 0x5D, 0x26, 0x30, 0x55, 0x3C, 0x27, 0x28 };

class NeteaseMusicMetadata {
private:
	std::string mAlbum;
	std::string mArtist;
	std::string mFormat;
	std::string mName;
	int mDuration = 0;
	int mBitrate = 0;

private:
	cJSON* mRaw = nullptr;

public:
	NeteaseMusicMetadata(cJSON* raw) {
		if (!raw) return;
		else this->mRaw = raw;

		auto swap = cJSON_GetObjectItem(raw, "musicName");
		if (swap) this->mName = std::string(cJSON_GetStringValue(swap));

		swap = cJSON_GetObjectItem(raw, "album");
		if (swap) this->mAlbum = std::string(cJSON_GetStringValue(swap));

		swap = cJSON_GetObjectItem(raw, "artist");
		if (swap)
			for (auto i = 0; i < cJSON_GetArraySize(swap) - 1; i++)
				this->mArtist += std::string(cJSON_GetStringValue(cJSON_GetArrayItem(cJSON_GetArrayItem(swap, i), 0))) + "/";

		swap = cJSON_GetObjectItem(raw, "bitrate");
		if (swap) this->mBitrate = swap->valueint;

		swap = cJSON_GetObjectItem(raw, "duration");
		if (swap) this->mDuration = swap->valueint;

		swap = cJSON_GetObjectItem(raw, "format");
		if (swap) this->mFormat = std::string(cJSON_GetStringValue(swap));
	}

	~NeteaseMusicMetadata() {
		cJSON_Delete(this->mRaw);
	}

	const std::string& name() const { return mName; }
	const std::string& album() const { return mAlbum; }
	const std::string& artist() const { return mArtist; }
	const std::string& format() const { return mFormat; }
	const int duration() const { return mDuration; }
	const int bitrate() const { return mBitrate; }
};

class NeteaseCrypt {
private:
	enum class NcmFormat { MP3, FLAC };
	unsigned char mKeyBox[256] = { 0 };
	std::string mFilepath;
	std::string mDumpFilepath;
	NcmFormat mFormat;
	std::string mImageData;
	std::ifstream mFile;
	NeteaseMusicMetadata* mMetaData;

private:
	bool isNcmFile() {
		char header[9] = { 0 };
		this->mFile.read(header, 8);
		return std::string(header) == "CTENFDAM";
	}

	bool openFile(std::string const& path) {
		try { mFile.open(path, std::ios::in | std::ios::binary); }
		catch (...) { return false; }
		return true;
	}

	int read(char* s, std::streamsize n) {
		mFile.read(s, n);
		auto gcount = mFile.gcount();
		if (gcount <= 0) { throw std::invalid_argument("can't read file"); }
		return gcount;
	}

	void buildKeyBox(unsigned char* key_data, int keyLen) {
		for (auto i = 0; i < 256; ++i)mKeyBox[i] = (unsigned char)i;

		unsigned char swap = 0;
		unsigned char c = 0;
		unsigned char last_byte = 0;
		unsigned char key_offset = 0;

		for (auto i = 0; i < 256; ++i) {
			swap = mKeyBox[i];
			c = ((swap + last_byte + key_data[key_offset++]) & 0xff);
			if (key_offset >= keyLen) key_offset = 0;
			mKeyBox[i] = mKeyBox[c];
			mKeyBox[c] = swap;
			last_byte = c;
		}
	}

	std::string mimeType(std::string& data) { return data == std::string(mPng) ? "image/png" : "image/jpeg"; }

public:
	const std::string& filepath() const { return mFilepath; }
	const std::string& dumpFilepath() const { return mDumpFilepath; }

public:
	~NeteaseCrypt() {
		if (mMetaData != NULL) delete mMetaData;
		mFile.close();
	}

	NeteaseCrypt(std::string const& path) {
		this->mFilepath = path;

		if (!(openFile(path) && isNcmFile() && mFile.seekg(2, mFile.cur))) throw std::invalid_argument("can't open file");

		int_fast32_t key_length;
		read(reinterpret_cast<char*>(&key_length), 4);
		if (key_length <= 0) throw std::invalid_argument("broken ncm file");

		auto key_data = new char[key_length];
		read(key_data, key_length);
		for (auto i = 0; i < key_length; i++) key_data[i] ^= 0x64;

		std::string recovered_key;
		std::string temp(key_data, key_length);
		CryptoPP::ECB_Mode<CryptoPP::AES>::Decryption d;
		d.SetKey(core_key, sizeof(core_key));
		CryptoPP::StringSource(temp, true, new CryptoPP::StreamTransformationFilter(d, new CryptoPP::StringSink(recovered_key)));

		recovered_key = recovered_key.substr(17, recovered_key.length());
		buildKeyBox((unsigned char*)recovered_key.c_str(), recovered_key.length());

		int_fast32_t meta_length;
		read(reinterpret_cast<char*>(&meta_length), 4);

		if (meta_length <= 0) this->mMetaData = NULL;
		else {
			auto meta_data = new char[meta_length];
			read(meta_data, meta_length);
			for (auto i = 0; i < meta_length; i++) meta_data[i] ^= 0x63;

			std::string decoded_meta, recovered_meta;
			temp = std::string(meta_data + 22, meta_length - 22);

			CryptoPP::StringSource(temp, true, new CryptoPP::Base64Decoder(new CryptoPP::StringSink(decoded_meta)));
			d.SetKey(meta_key, sizeof(meta_key));
			CryptoPP::StringSource(decoded_meta, true, new CryptoPP::StreamTransformationFilter(d, new CryptoPP::StringSink(recovered_meta)));
			recovered_meta = recovered_meta.substr(6, recovered_meta.length());

			this->mMetaData = new NeteaseMusicMetadata(cJSON_Parse(recovered_meta.c_str()));

			delete[] meta_data;
		}

		// skip crc32 & unuse charset
		if (!mFile.seekg(9, mFile.cur)) throw std::invalid_argument("can't seek file");

		int_fast32_t image_size;
		read(reinterpret_cast<char*>(&image_size), 4);

		if (image_size > 0) {
			auto image_data = new char[image_size];
			read(image_data, image_size);
			this->mImageData = std::string(image_data, image_size);
			delete[] image_data;
		}

		delete[] key_data;
	}

public:
	void Dump() {
		std::ofstream output;
		auto buffer = new char[0xF000];
		std::filesystem::path file_path(mFilepath);

		for (uint_fast16_t n = 0xF000; !mFile.eof(); output.write((char*)buffer, n)) {
			n = read(buffer, n);

			for (auto i = 0; i < n; i++) {
				auto j = (i + 1) & 0xff;
				buffer[i] ^= mKeyBox[(mKeyBox[j] + mKeyBox[(mKeyBox[j] + j) & 0xff]) & 0xff];
			}

			if (!output.is_open()) {
				if (buffer[0] == 0x49 && buffer[1] == 0x44 && buffer[2] == 0x33) {
					this->mDumpFilepath = file_path.replace_extension(".mp3").string();
					mFormat = NeteaseCrypt::NcmFormat::MP3;
				}
				else {
					this->mDumpFilepath = file_path.replace_extension(".flac").string();
					mFormat = NeteaseCrypt::NcmFormat::FLAC;
				}
				output.open(mDumpFilepath, output.out | output.binary);
			}
		}

		output.flush();
		output.close();

		delete[] buffer;
	}

	void FixMetadata() {
		if (mDumpFilepath.length() <= 0) { throw std::invalid_argument("must dump before"); }
		TagLib::File* audioFile = nullptr;
		TagLib::Tag* tag = nullptr;
		TagLib::ByteVector vector(mImageData.c_str(), mImageData.length());
		if (this->mFormat == NeteaseCrypt::NcmFormat::MP3) {
			audioFile = new TagLib::MPEG::File(mDumpFilepath.c_str());
			tag = dynamic_cast<TagLib::MPEG::File*>(audioFile)->ID3v2Tag(true);
			if (mImageData.length() > 0) {
				TagLib::ID3v2::AttachedPictureFrame* frame = new TagLib::ID3v2::AttachedPictureFrame;
				frame->setMimeType(mimeType(mImageData));
				frame->setPicture(vector);
				dynamic_cast<TagLib::ID3v2::Tag*>(tag)->addFrame(frame);
			}
		}
		else if (mFormat == NeteaseCrypt::NcmFormat::FLAC) {
			audioFile = new TagLib::FLAC::File(mDumpFilepath.c_str());
			tag = audioFile->tag();
			if (mImageData.length() > 0) {
				TagLib::FLAC::Picture* cover = new TagLib::FLAC::Picture;
				cover->setMimeType(mimeType(mImageData));
				cover->setType(TagLib::FLAC::Picture::FrontCover);
				cover->setData(vector);
				dynamic_cast<TagLib::FLAC::File*>(audioFile)->addPicture(cover);
			}
		}
		if (mMetaData != NULL) {
			tag->setTitle(TagLib::String(mMetaData->name(), TagLib::String::UTF8));
			tag->setArtist(TagLib::String(mMetaData->artist(), TagLib::String::UTF8));
			tag->setAlbum(TagLib::String(mMetaData->album(), TagLib::String::UTF8));
		}
		tag->setComment(TagLib::String("Create by netease copyright protected dump tool. author 5L", TagLib::String::UTF8));
		audioFile->save();
	}
};

inline void visite_dir(std::filesystem::directory_iterator list) {
	try {
#if _OPENMP
#define ST
#else
#pragma omp parallel for
#endif
		for (auto& it : list) {
			if (it.status().type() == std::filesystem::file_type::directory) {
				std::filesystem::directory_iterator list(it);
				visite_dir(list);
			}
			else if (it.path().extension() == ".ncm") {
				NeteaseCrypt crypt(it.path().string());
				crypt.Dump();
				crypt.FixMetadata();
				std::cout << crypt.dumpFilepath() << std::endl;
			}
		}
	}
	catch (std::system_error e) {
		std::cout << "tips: " << e.what() << " ";
		std::cout << "出现此问题通常是因为歌名中存在特殊字符，由于c++17的filesystem的缺陷，暂时无法解决！" << std::endl;
	}
	catch (std::exception e) {
		std::cout << "tips: " << e.what() << std::endl;
		std::exit(3);
	} 
	catch (...) {
		std::cout << "tips: " << "未知错误，请联系开发者！" << std::endl;
		std::exit(4);
	}
}