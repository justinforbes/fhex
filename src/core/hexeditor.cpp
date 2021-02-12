#include "hexeditor.h"

/*
 * REMOVED HARDCODED PATTERNS FILE PATH
 */

HexEditor::HexEditor()
{
    this->fileSize = 0;
    this->loadedFileSize = 0;
    this->patternMatching = NULL;
}

HexEditor::HexEditor(string patternsFile)
{
    HexEditor();
    this->patternMatching = new PatternMatching(patternsFile);
}

/*
 * ADDED patternsFile TO MAKE SIGNATURE DIFFERENT
 */

HexEditor::HexEditor(string path, string patternsFile)
{
    (HexEditor(patternsFile));
    this->loadFileAsync(path);
}

HexEditor::~HexEditor()
{
    delete this->patternMatching;
}

void HexEditor::loadFilePart(string path, unsigned long start, unsigned long offset) {
    ifstream ifs(path, ios::binary);
    ifs.seekg(start, ios::beg);
    long block_size;
    while (this->bytesRead < offset) {
        block_size = 10485760; //10 MB
        if (this->bytesRead + block_size > offset)
            block_size = offset - this->bytesRead;
        ifs.read(reinterpret_cast<char*>(this->current_data.data())+this->bytesRead, block_size);
        this->bytesRead += block_size;
    }
}

bool HexEditor::isFileLoaded() {
    return (this->bytesRead == this->loadedFileSize);
}

bool HexEditor::loadFileAsync(string path, unsigned long start, unsigned long offset) {
    std::ifstream ifs(path, std::ios::binary);

    if (!ifs.good()) {
        cerr << "The file " << path << " is not accessible." << endl;
        return false;
    }

    this->current_path = path;

    //clear current data
    this->current_data.clear();
    this->current_data.shrink_to_fit();

    ifs.seekg(0, std::ios::end);
    this->fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    if (start >= this->fileSize) {
        cerr << "Cannot read file from offset: " << start << endl;
        return false;
    }

    this->startOffset = start;
    if (offset == 0)
        offset = this->fileSize - start;

    if ((start + offset) > this->fileSize)
        offset = this->fileSize - start;

    this->loadedFileSize = offset;

    this->current_data.reserve(offset);
    this->bytesRead = 0;
    // copies data into buffer
    std::thread loader([this, path, start, offset](){ return this->loadFilePart(path, start, offset); });
    loader.detach();

    return true;
}


vector<uint8_t> &HexEditor::getCurrentData() {
    return this->current_data;
}

string HexEditor::getCurrentDataAsString(unsigned long start, unsigned long len) {
    std::string str(this->current_data.begin() + start, this->current_data.begin() + start + len);
    return str;
}

wstring HexEditor::getCurrentDataAsWString(unsigned long start, unsigned long len) {
    wstring str;
    for (auto it = this->getCurrentData().begin() + start; it != this->getCurrentData().begin() + start + len; ++it) {
        str.push_back(*it);
    }
    return str;
}

void HexEditor::setCurrentData(vector<uint8_t> &data) {
    this->current_data = data;
}

string HexEditor::getCurrentPath() {
    return this->current_path;
}

void HexEditor::updateByte(uint8_t new_byte, unsigned long file_offset) {
    this->current_data[file_offset] = new_byte;
    fstream out(this->current_path, ios::binary | ios::out | ios::in);
    out.seekp(static_cast<long>(file_offset), ios::beg);
    const char *b = reinterpret_cast<const char*>(&new_byte);
    out.write(b, 1);
}

bool HexEditor::saveDataToFile(string path) { //blocking function
    bool temp_file = false;
    //Check if we are trying to overwrite the file
    if (this->current_path == path) {
        //Use a temporary file
        path = path + ".fhextmp";
        temp_file = true;
    }
    ofstream fout(path, ios::out | ios::binary);
    if (!fout.good()) {
        cerr << "The file " << path << " is not accessible." << endl;
        return false;
    }
    if (this->loadedFileSize < this->fileSize) {
        //Copy initial and final part from original file, read the current chunk from the hexeditor
        std::ifstream ifs(this->current_path, std::ios::binary);
        ifs.seekg(0, std::ios::beg);
        vector<unsigned char> data;
        long block_size;
        unsigned long bytes_read = 0;
        while (bytes_read < this->startOffset) {
            block_size = 10485760; //10 MB
            if (bytes_read + block_size > this->startOffset)
                block_size = this->startOffset - bytes_read;
            data.reserve(block_size);
            data.clear();
            ifs.read(reinterpret_cast<char*>(data.data()), block_size);
            fout.write(reinterpret_cast<char*>(data.data()), block_size);
            bytes_read += block_size;
        }
        fout.write(reinterpret_cast<char*>(this->current_data.data()), this->loadedFileSize);
        unsigned long offset = this->fileSize - (this->startOffset + this->loadedFileSize);
        if (offset > 0) {
            //Copy the final part from the original file
            ifs.seekg((this->startOffset + this->loadedFileSize), ios::beg);
            bytes_read = 0;
            while (bytes_read < offset) {
                block_size = 10485760; //10 MB
                if (bytes_read + block_size > offset)
                    block_size = offset - bytes_read;
                data.reserve(block_size);
                data.clear();
                ifs.read(reinterpret_cast<char*>(data.data()), block_size);
                fout.write(reinterpret_cast<char*>(data.data()), block_size);
                bytes_read += block_size;
            }
         }
    } else {
        fout.write(reinterpret_cast<char*>(this->current_data.data()), this->loadedFileSize);
    }
    fout.close();
    // if we used a temporary file, move it to the original path
    if (temp_file) {
        remove(this->current_path.c_str());
        rename(path.c_str(), this->current_path.c_str());
    }
    return true;
}

string HexEditor::fromUintVectorToPrintableString(vector<uint8_t> &vec, long start, long len) {
    stringstream ss;
    vector<uint8_t>::const_iterator it;

    for (it = vec.begin() + start; it != (vec.begin() + start + len); it++) {
        char c = static_cast<char>(*it);
        if (isprint(c)) {
            ss << c;
        } else {
            ss << ".";
        }
    }

    return ss.str();
}

vector<Match *> HexEditor::findPatterns() {
    return this->patternMatching->hasMatches(this->current_data);
}

vector<pair<unsigned long, uint8_t>>  HexEditor::compareTo(HexEditor &hexEditor) {
    vector<pair<unsigned long, uint8_t>> diff_bytes;

    //Find who has the smaller file size
    unsigned long filesize = this->loadedFileSize;
    if (hexEditor.loadedFileSize < filesize)
        filesize = hexEditor.loadedFileSize;

    for (unsigned long i = 0; i < filesize; i++) {
        uint8_t byte_new = hexEditor.getCurrentData()[i];
        if (this->getCurrentData()[i] != byte_new) {
            diff_bytes.push_back(pair<unsigned long, uint8_t>(i, byte_new));
        }
        this->bytesRead = i;
    }

    return diff_bytes;
}
