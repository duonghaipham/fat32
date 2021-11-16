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
#include <algorithm>

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
    void GetFileInfo(BYTE sector[], int firstCluster);
    vector<int> GetFileClusters(int firstCluster);
    vector<int> GetFileSectors(vector<int> fileClusters);
public:
    FAT32(LPCWSTR drive);
    void GetInfo();
    void GetDirectory(int readPoint);
    int GetRootClus();
};

wstring rtrim(const wstring& ws) {
    size_t end = ws.find_last_not_of(' ');
    return (end == wstring::npos) ? L"" : ws.substr(0, end + 1);
}

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
    readPoint += BYTES_READ;

    int clusterValue = firstCluster;
    do {
        fileClusters.push_back(clusterValue);
        clusterValue = GetIntValue(sector, (4 * clusterValue) % 512, 4);

        if (clusterValue > readPoint / 4) {
            SetFilePointer(device, readPoint, NULL, FILE_BEGIN);
            ReadSector(readPoint, sector);
            readPoint += BYTES_READ;
        }
    } while (clusterValue != STOP_CLUSTER);

    return fileClusters;
}

vector<int> FAT32::GetFileSectors(vector<int> fileClusters) {
    vector<int> fileSectors;
    
    for (unsigned int i = 0; i < fileClusters.size(); i++) {
        int firstSector = FindFirstSectorOfCluster(fileClusters[i]);

        for (int i = 0; i < secPerClus; i++)
            fileSectors.push_back(firstSector + i);
    }

    return fileSectors;
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

void FAT32::GetFileInfo(BYTE sector[], int firstCluster) {
    vector<int> fileClusters = GetFileClusters(firstCluster);
    vector<int> fileSectors = GetFileSectors(fileClusters);

    wcout << "Cluster bat dau: " << firstCluster << endl;
    wcout << "Chiem cac cluster: ";

    for (unsigned int i = 0; i < fileClusters.size(); i++)
        wcout << fileClusters[i] << " ";
    wcout << endl;

    wcout << "Chiem cac sector: ";
    for (unsigned int i = 0; i < fileSectors.size(); i++)
        wcout << fileSectors[i] << " ";
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
                    GetFileInfo(sector, firstCluster);
                    GetDirectory(firstCluster);
                }
            }

            if (sector[index + 11] == 32) {
                wstring fileExtension;

                if (totalEntryName.size() == 0) {
                    wstring fileName = rtrim(GetStringValue(sector, index, 8, false));
                    fileExtension = rtrim(GetStringValue(sector, index + 8, 4, false));

                    wstring mainEntryName = (fileExtension == L"") ? fileName : fileName + L"." + fileExtension;

                    wprintf(L"%s\n", mainEntryName.c_str());
                }
                else {
                    int dotIndex = totalEntryName.rfind(L'.');
                    fileExtension = (dotIndex == wstring::npos) ? L"" :
                        totalEntryName.substr(dotIndex + 1, totalEntryName.length() - dotIndex - 1);

                    wprintf(L"%s\n", totalEntryName.c_str());
                }

                int firstCluster = GetIntValue(sector, index + 26, 2) +
                    (int) pow(16, 2) * GetIntValue(sector, index + 20, 2);

                if (firstCluster != 0)
                    GetFileInfo(sector, firstCluster);

                int fileSize = GetIntValue(sector, index + 28, 4) * bytsPerSec;
                wcout << "Kich co: " << fileSize << " B" << endl;

                transform(fileExtension.begin(), fileExtension.end(), fileExtension.begin(), ::toupper);

                if (wcscmp(fileExtension.c_str(), L"TXT") == 0) {
                    if (firstCluster != 0) {
                        vector<int> fileClusters = GetFileClusters(firstCluster);
                        vector<int> fileSectors = GetFileSectors(fileClusters);

                        for (unsigned int i = 0; i < fileSectors.size(); i++) {
                            BYTE sectorFile[BYTES_READ];

                            int readPointFile = fileSectors[i] * bytsPerSec;

                            SetFilePointer(device, readPointFile, NULL, FILE_BEGIN);
                            ReadSector(readPointFile, sectorFile);

                            for (int j = 0; j < BYTES_READ && sectorFile[j] != '\0'; j += 1)
                                wprintf(L"%c", sectorFile[j]);
                        }
                    }
                    wprintf(L"\n");
                }
                else
                    wprintf(L"Can phan mem khac de doc file khac .txt\n");
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
