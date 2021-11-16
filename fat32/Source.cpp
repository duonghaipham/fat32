#include <windows.h>
#include <iostream>
#include <stdio.h>
#include <math.h>
#include <string>
#include <wchar.h>
#include <vector>
#include <locale>
#include <fcntl.h>
#include <io.h>

#define START_POINT_FILE 0
#define BYTES_READ 512
#define STOP_CLUSTER 268435455

#pragma warning (disable: 4996)

#ifdef __cplusplus
extern "C" {
#endif
BOOL WINAPI SetCurrentConsoleFontEx(HANDLE hConsoleOutput, BOOL bMaximumWindow, PCONSOLE_FONT_INFOEX
lpConsoleCurrentFontEx);
#ifdef __cplusplus
}
#endif

using namespace std;

class FAT32 {
private:
    LPCWSTR drive;
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
    wstring GetStringValue(BYTE sector[], int offset, int size, bool isShort);
    int FindFirstSectorOfCluster(int cluster);
    void GetFileInfo(BYTE sector[], int firstCluster, bool isFile);
    vector<int> GetFileClusters(int firstCluster);
    vector<int> GetFileSectors();
public:
    FAT32(LPCWSTR drive);
    void GetInfo();
    void GetDirectory(int readPoint);
    int GetRootClus();
};

FAT32::FAT32(LPCWSTR drive) {
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
    return 0;
}

int FAT32::GetIntValue(BYTE sector[], int offset, int size) {
    int value = 0;
    int exp = 0;
    for (int i = offset; i < offset + size; i++) {
        value += (int) pow(16, exp) * sector[i];
        exp += 2;
    }
    return value;
}

wstring FAT32::GetStringValue(BYTE sector[], int offset, int size, bool isShort) { // short: 2 bytes
    wstring value;
    for (int i = offset; i < offset + size - 1;) {
        int intValue = 0;
        if (isShort) {
            intValue = GetIntValue(sector, i, 2);
            i += 2;
        }
        else {
            intValue = GetIntValue(sector, i, 1);
            i++;
        }

        wchar_t currChar = static_cast<wchar_t>(intValue);
        value += currChar;
    }
    return value;
}

int FAT32::GetRootClus() {
    return rootClus;
}

int FAT32::FindFirstSectorOfCluster(int cluster) {
    return (cluster - 2) * secPerClus + rsvdSecCnt + numFATs * fATSz32;
}

vector<int> FAT32::GetFileClusters(int firstCluster) {
    vector<int> fileClusters;

    BYTE sector[BYTES_READ];
    int readPoint = rsvdSecCnt * bytsPerSec;

    SetFilePointer(device, readPoint, NULL, FILE_BEGIN);
    ReadSector(readPoint, sector);

    readPoint += 512;


    int clusterValue = firstCluster;

    do {
        fileClusters.push_back(clusterValue);
        clusterValue = GetIntValue(sector, 4 * clusterValue, 4);
    } while (clusterValue != STOP_CLUSTER);

    return fileClusters;
}

void FAT32::GetInfo() {
    wcout << "So bytes/sector: " << bytsPerSec << endl;
    wcout << "So sector/cluster: " << secPerClus << endl;;
    wcout << "So sector Reserved: " << rsvdSecCnt << endl;
    wcout << "So bang FAT: " << numFATs << endl;
    wcout << "So sector cua mot bang FAT: " << fATSz32 << endl;
    wcout << "Tong sector volume: " << totSec32 << endl;
    wcout << "Dia chi sector dau tien bang FAT1: " << rsvdSecCnt << endl;
    wcout << "Dia chi sector dau tien Data: " << rsvdSecCnt + numFATs * fATSz32 << endl;
}

void FAT32::GetFileInfo(BYTE sector[], int firstCluster, bool isFile) {
    vector<int> fileClusters = GetFileClusters(firstCluster);
//    vector<int> fileSectors = GetFileSectors();

    wcout << "Cluster bat dau: " << firstCluster << endl;
    wcout << "Chiem cac cluster: ";

    for (unsigned int i = 0; i < fileClusters.size(); i++)
        wcout << fileClusters[i] << " ";
    wcout << endl;
}

void FAT32::GetDirectory(int cluster) {
    BYTE sector[BYTES_READ];
    int readPoint = FindFirstSectorOfCluster(cluster) * bytsPerSec;

    do {
        SetFilePointer(device, readPoint, NULL, FILE_BEGIN);
        ReadSector(readPoint, sector);

        readPoint += 512;

        int index = 0;

        wstring totalEntryName;
        while (index < 512) {
            if (sector[index + 11] == 16) {
                if (sector[index] != '.') {
                    if (totalEntryName.size() == 0) {
                        wstring mainEntryName = GetStringValue(sector, index, 12, false);
                        wprintf(L"%s\n", mainEntryName.c_str());
                    }
                    else
                        wprintf(L"%s\n", totalEntryName.c_str());

                    int firstCluster = GetIntValue(sector, index + 26, 2) +
                        (int) pow(16, 2) * GetIntValue(sector, index + 20, 2);
                    GetFileInfo(sector, firstCluster, false);
                    GetDirectory(firstCluster);
                }
            }

            if (sector[index + 11] == 32) {
                if (totalEntryName.size() == 0) {
                    wstring mainEntryName = GetStringValue(sector, index, 12, false);
                    wprintf(L"%s\n", mainEntryName.c_str());
                }
                else
                    wprintf(L"%s\n", totalEntryName.c_str());

                int firstCluster = GetIntValue(sector, index + 26, 2) +
                    (int) pow(16, 2) * GetIntValue(sector, index + 20, 2);
                GetFileInfo(sector, firstCluster, true);
            }

            if (sector[index + 11] == 15) {
                wstring extraEntryName =
                    GetStringValue(sector, index + 1, 10, true) +
                    GetStringValue(sector, index + 14, 12, true) +
                    GetStringValue(sector, index + 28, 4, true);

                totalEntryName = extraEntryName + totalEntryName;
            }
            else
                totalEntryName.clear();

            index += 32;
        }
    } while (sector[0] != 0);
}

void ConfigureConsoleLayout() {
    CONSOLE_FONT_INFOEX cfi;
    cfi.cbSize = sizeof cfi;
    cfi.nFont = 0;
    cfi.dwFontSize.X = 0;
    cfi.dwFontSize.Y = 16;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;

    wcscpy(cfi.FaceName, L"Consolas");
    SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &cfi);

    _setmode(_fileno(stdout), 0x00020000);
}

int main(int argc, char** argv) {
    ConfigureConsoleLayout();
    FAT32 fat32(L"\\\\.\\E:");
    //fat32.GetInfo();
    int rootClus = fat32.GetRootClus();

    fat32.GetDirectory(rootClus);
    return 0;
}
