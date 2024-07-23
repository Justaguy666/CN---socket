#include "pch.h"
#include "framework.h"
#include "Client.h"
#include <afxsock.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <signal.h>
#include <filesystem>
#include <WS2tcpip.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CWinApp theApp;

using namespace std;

struct FileInfo {
    string name;
    streamsize size;
};

bool stop = false;

void signalHandler(int signum) {
    cout << "\nInterrupt signal (" << signum << ") received. Exiting..." << endl;
    stop = true;
}

void DownloadFile(CSocket& clientSocket, const string& fileName) {
    cout << "Requesting file: " << fileName << endl;

    clientSocket.Send(fileName.c_str(), fileName.size());

    streamsize fileSize;
    clientSocket.Receive(&fileSize, sizeof(fileSize));

    if (fileSize < 0) {
        cout << "File not found on server: " << fileName << endl;
        return;
    }

    const string outputDir = "output/";
    if (!filesystem::exists(outputDir)) {
        if (!filesystem::create_directory(outputDir)) {
            cerr << "Error creating output directory." << endl;
            return;
        }
    }

    ofstream outputFile(outputDir + fileName, ios::binary);
    if (!outputFile.is_open()) {
        char errorMsg[256];
        strerror_s(errorMsg, sizeof(errorMsg), errno);
        cerr << "Error opening output file: " << outputDir + fileName << ". Error: " << errorMsg << endl;
        return;
    }

    char buffer[1024];
    streamsize totalBytesReceived = 0;
    while (totalBytesReceived < fileSize && !stop) {
        int bytesReceived = clientSocket.Receive(buffer, sizeof(buffer));
        if (bytesReceived == 0) {
            cerr << "Connection closed by server." << endl;
            break;
        }

        outputFile.write(buffer, bytesReceived);
        totalBytesReceived += bytesReceived;

        float progress = ((totalBytesReceived * 100.0) / fileSize);
        cout << "Downloading " << fileName << " .... " << progress << "%" << "\r";
        cout.flush();
    }
    outputFile.close();
    if (!stop) {
        cout << "Downloading " << fileName << " .... 100%" << endl;
    }
}

int main() {
    signal(SIGINT, signalHandler);

    int nRetCode = 0;
    HMODULE hModule = ::GetModuleHandle(nullptr);

    if (hModule != nullptr) {
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0)) {
            wprintf(L"Fatal Error: MFC initialization failed\n");
            nRetCode = 1;
            system("pause");
            return nRetCode;
        }
        else {
            if (AfxSocketInit() == false) {
                cerr << "Fail to initialize sockets.\n";
                system("pause");
                return 1;
            }

            CSocket clientSocket;
            if (!clientSocket.Create()) {
                cerr << "Fail to create client socket.\n";
                system("pause");
                return 1;
            }

            char IP[1000];
            unsigned int port;
            cout << "Enter server IP address: ";
            cin >> IP;
            cout << "Enter server port: ";
            cin >> port;

            if (!clientSocket.Connect((CA2W)IP, port)) {
                cerr << "Fail to connect to server.\n";
                clientSocket.Close();
                system("pause");
                return 1;
            }

            cout << "Connected to server.\n";

            int numFiles;
            clientSocket.Receive(&numFiles, sizeof(numFiles));

            vector<FileInfo> files(numFiles);

            for (int i = 0; i < numFiles; ++i) {
                int len;
                clientSocket.Receive(&len, sizeof(len));

                char* namebuffer = new char[len + 1];
                clientSocket.Receive(namebuffer, len);
                namebuffer[len] = '\0';

                streamsize size;
                clientSocket.Receive(&size, sizeof(size));

                files[i] = { string(namebuffer), size };
                delete[] namebuffer;
            }

            cout << "Files available for download:\n";
            for (const auto& file : files) {
                cout << file.name << " " << file.size << " Bytes\n";
            }

            for (const auto& file : files) {
                if (stop) break;
                DownloadFile(clientSocket, file.name);
            }

            clientSocket.Close();
        }
    }
    else {
        wprintf(L"Fatal Error: GetModuleHandle failed\n");
        nRetCode = 1;
        system("pause");
    }

    system("pause");

    return nRetCode;
}
