#include <windows.h>
#include <iostream>
#include <stdio.h>
#include <math.h>
#include <string>
#include <wchar.h>
#include <vector>
#include <sstream>

#include <io.h>
#include <fcntl.h>

#define START_POINT_FILE 0
#define BYTES_READ 512

using namespace std;

vector<wchar_t> operator+(vector<wchar_t> lhs, vector<wchar_t> rhs) {
    vector<wchar_t> result;
    result.reserve(lhs.size() + rhs.size());
    result.insert(result.end(), lhs.begin(), lhs.end());
    result.insert(result.end(), rhs.begin(), rhs.end());
    return result;
}

wchar_t* ConvertVectorToUniString(vector<wchar_t> v) {
    wchar_t* result = new wchar_t[v.size()];

    for (unsigned int i = 0; i < v.size(); i++)
        result[i] = v[i];
    return result;
}

class FAT32 {
private:
    LPCSTR drive;
    HANDLE device;
    int bytsPerSec;
    int secPerClus;
    int rsvdSecCnt;
    int numFATs;
    int totSec32;
    int fATSz32;
    int rootClus;
    int ReadSector(int readPoint, BYTE sector[]);
    int GetIntValue(BYTE sector[], int offset, int size);
    vector<wchar_t> GetStringValue(BYTE sector[], int offset, int size, bool isShort);
    int FindFirstSectorOfCluster(int cluster);
    void PrintUni(vector<wchar_t> output);
public:
    FAT32(LPCSTR drive);
    void GetInfo();
    void GetDirectory();
};

FAT32::FAT32(LPCSTR drive) {
    this->drive = drive;

    device = CreateFile(drive,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    BYTE sector[BYTES_READ];

    if (ReadSector(START_POINT_FILE, sector) == 0) {
        bytsPerSec = GetIntValue(sector, 11, 2);
        secPerClus = GetIntValue(sector, 13, 1);
        rsvdSecCnt = GetIntValue(sector, 14, 2);
        numFATs = GetIntValue(sector, 16, 1);
        totSec32 = GetIntValue(sector, 19, 2) == 0 ? GetIntValue(sector, 32, 4) : GetIntValue(sector, 19, 2);
        fATSz32 = GetIntValue(sector, 36, 4);
        rootClus = GetIntValue(sector, 44, 4);
    }
}

int FAT32::ReadSector(int readPoint, BYTE sector[]) {
    DWORD bytesRead;

    if (device == INVALID_HANDLE_VALUE)
        return 1;

    SetFilePointer(device, readPoint, NULL, FILE_BEGIN);

    if (!ReadFile(device, sector, BYTES_READ, &bytesRead, NULL))
        return 1;
    else
        return 0;
}

int FAT32::GetIntValue(BYTE* sector, int offset, int size) {
    int value = 0;
    int exp = 0;
    for (int i = offset; i < offset + size; i++) {
        value += (int) pow(16, exp) * sector[i];
        exp += 2;
    }
    return value;
}

vector<wchar_t> FAT32::GetStringValue(BYTE sector[], int offset, int size, bool isShort) {
    vector<wchar_t> value;
    for (int i = offset; i < offset + size - 1;) {
        int intValue;
        if (isShort) {
            intValue = GetIntValue(sector, i, 2);
            i += 2;
        }
        else {
            intValue = GetIntValue(sector, i, 1);
            i++;
        }

        wchar_t currChar = static_cast<wchar_t>(intValue);
        value.push_back(currChar);
    }
    return value;
}

void FAT32::PrintUni(vector<wchar_t> output) {
    for (unsigned int i = 0; i < output.size(); i++)
        wcout << output[i];
}

int FAT32::FindFirstSectorOfCluster(int cluster) {
    return (cluster - 2) * secPerClus + rsvdSecCnt + numFATs * fATSz32;
}

void FAT32::GetInfo() {
    cout << "So bytes/sector: " << bytsPerSec << endl;
    cout << "So sector/cluster: " << secPerClus << endl;;
    cout << "So sector Reserved: " << rsvdSecCnt << endl;
    cout << "So bang FAT: " << numFATs << endl;
    cout << "So sector cua mot bang FAT: " << fATSz32 << endl;
    cout << "Tong sector volume: " << totSec32 << endl;
    cout << "Dia chi sector dau tien bang FAT1: " << rsvdSecCnt << endl;
    cout << "Dia chi sector dau tien Data: " << rsvdSecCnt + numFATs * fATSz32 << endl;
}

void FAT32::GetDirectory() {
    BYTE sector[BYTES_READ];
    int readPoint = FindFirstSectorOfCluster(rootClus) * bytsPerSec;

    do {
        SetFilePointer(device, readPoint, NULL, FILE_BEGIN);
        ReadSector(readPoint, sector);

        readPoint += 512;

        int index = 11;

        vector<wchar_t> totalEntryName;
        while (index < 512) {
            if (sector[index] == 32) {
                if (totalEntryName.size() == 0) {
                    vector<wchar_t> mainEntryName = GetStringValue(sector, index - 11, 11, false);
                    PrintUni(mainEntryName);
                }
                else {
                    PrintUni(totalEntryName);
//                    totalEntryName.clear();
                }
            }

            if (sector[index] == 15) {
                vector<wchar_t> extraEntryName =
                    GetStringValue(sector, index - 11 + 1, 10, true) +
                    GetStringValue(sector, index - 11 + 14, 12, true) +
                    GetStringValue(sector, index - 11 + 28, 4, true);

                totalEntryName = extraEntryName + totalEntryName;
            }

            if (sector[index] == 16) {
                if (totalEntryName.size() == 0) {
                    vector<wchar_t> mainEntryName = GetStringValue(sector, index - 11, 11, false);
                    PrintUni(mainEntryName);
                }
                else {
                    PrintUni(totalEntryName);
//                    totalEntryName.clear();
                }
            }
            index += 32;
        }
    } while (sector[0] != 0);
}

int main(int argc, char** argv)
{
    _setmode(_fileno(stdout), 0x00020000);
    FAT32 fat32("\\\\.\\F:");
//    fat32.GetInfo();
    fat32.GetDirectory();
    return 0;
}
