#include "pch.h"
#include "framework.h"
#include "Server.h"
#include <afxsock.h>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <cmath>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CWinApp theApp;

using namespace std;

struct FileInfo {
    string name;
    streamsize size;
};

streamsize ConvertToBytes(const string& sizeStr) {
    string unit;
    streamsize size;
    istringstream iss(sizeStr);
    iss >> size >> unit;

    if (unit == "KB") size *= 1024;
    else if (unit == "MB") size *= pow(1024, 2);
    else if (unit == "GB") size *= pow(1024, 3);
    else if (unit == "TB") size *= pow(1024LL, 4);

    return size;
}

vector<FileInfo> ReadFileList(const string& filename) {
    vector<FileInfo> files;
    ifstream inFile(filename);

    if (inFile.is_open()) {
        string line;
        while (getline(inFile, line)) {
            size_t pos = line.find(' ');
            if (pos != string::npos) {
                string name = line.substr(0, pos);
                string sizeStr = line.substr(pos + 1);
                streamsize size = ConvertToBytes(sizeStr);

                files.push_back({ name, size });
            }
        }
        inFile.close();
    }
    else {
        cerr << "Could not open file list: " << filename << endl;
    }
    return files;
}

void HandleClient(CSocket* pClient, const vector<FileInfo>& files) {
    int numFiles = files.size();
    pClient->Send(&numFiles, sizeof(numFiles));

    for (const FileInfo& file : files) {
        int len = file.name.size();
        pClient->Send(&len, sizeof(len));
        pClient->Send(file.name.c_str(), file.name.size());
        pClient->Send(&file.size, sizeof(file.size));
    }

    char buffer[1024];
    while (true) {
        int bytesReceived = pClient->Receive(buffer, sizeof(buffer));
        if (bytesReceived == 0) {
            cerr << "Connection closed by client." << endl;
            break;
        }

        buffer[bytesReceived] = '\0';
        string fileName(buffer);

        ifstream file(fileName, ios::binary | ios::ate);
        if (file.is_open()) {
            streamsize fileSize = file.tellg();
            file.seekg(0, ios::beg);
            pClient->Send(&fileSize, sizeof(fileSize));

            char fileBuffer[1024];
            while (file.read(fileBuffer, sizeof(fileBuffer))) {
                pClient->Send(fileBuffer, sizeof(fileBuffer));
            }
            pClient->Send(fileBuffer, file.gcount());
            file.close();
        }
        else {
            streamsize fileSize = -1;
            pClient->Send(&fileSize, sizeof(fileSize));
        }
    }
    pClient->Close();
    delete pClient;
}

int main() {
    int nRetCode = 0;
    HMODULE hModule = ::GetModuleHandle(nullptr);

    if (hModule != nullptr) {
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0)) {
            wprintf(L"Fatal Error: MFC initialization failed\n");
            nRetCode = 1;
        }
        else {
            if (AfxSocketInit() == false) {
                cerr << "Fail to initialize sockets.\n";
                return 1;
            }

            CSocket serverSocket;
            if (!serverSocket.Create(12345)) {
                cerr << "Fail to create server socket.\n";
                return 1;
            }

            if (!serverSocket.Listen()) {
                cerr << "Fail to listen on server socket.\n";
                return 1;
            }

            cout << "Server is listening on port 12345.\n";
            vector<FileInfo> files = ReadFileList("input.txt");

            while (true) {
                CSocket* pClient = new CSocket;
                if (serverSocket.Accept(*pClient)) {
                    cout << "Client is connected.\n";
                    HandleClient(pClient, files);
                }
                else {
                    delete pClient;
                }
            }

            serverSocket.Close();
        }
    }
    else {
        wprintf(L"Fatal Error: GetModuleHandle failed\n");
        nRetCode = 1;
    }

    return nRetCode;
}