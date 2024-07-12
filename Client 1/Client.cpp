// Client.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "framework.h"
#include "Client.h"
#include <afxsock.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <signal.h>
#include <WS2tcpip.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// The one and only application object

struct FileInfo
{
    string name;
    streamsize size;
};

CWinApp theApp;

using namespace std;

bool stop = false;

void signalHandler(int signum)
{
    cout << "Interrupt signal (" << signum << ") received. Exiting..." << endl;
    stop = true;
}

void DownloadFile(CSocket& clientSocket, const string& fileName)
{
    clientSocket.Send(fileName.c_str(), fileName.size());
    streamsize fileSize;
    clientSocket.Receive(&fileSize, sizeof(fileSize));

    if (fileSize < 0)
    {
        cout << "File not found on server: " << fileName << endl;
        return;
    }

    ofstream outputFile("output/" + fileName, ios::binary);
    char buffer[1024];
    streamsize totalBytesReceived = 0;
    while (totalBytesReceived < fileSize && !stop)
    {
        int bytesReceived = clientSocket.Receive(buffer, sizeof(buffer));
        if (bytesReceived <= 0)
            break;

        outputFile.write(buffer, bytesReceived);
        totalBytesReceived += bytesReceived;

        // Display download progress
        float progress = ((totalBytesReceived * 100.0) / fileSize);
        cout << "Downloading " << fileName << " .... " << progress << "%" << "\r";
        cout.flush();
    }
    outputFile.close();
    if (!stop) {
        cout << "Downloading " << fileName << " .... 100%" << endl;
    }
}

int main()
{
    // Register signal handler for SIGINT
    signal(SIGINT, signalHandler);

    int nRetCode = 0;

    HMODULE hModule = ::GetModuleHandle(nullptr);

    if (hModule != nullptr)
    {
        // Initialize MFC and print an error on failure
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            wprintf(L"Fatal Error: MFC initialization failed\n");
            nRetCode = 1;
        }
        else
        {
            if (AfxSocketInit() == false)
            {
                cerr << "Fail to initialize sockets.\n";
                return 1;
            }

            CSocket clientSocket;
            if (!clientSocket.Create())
            {
                cerr << "Fail to create client socket.\n";
                return 1;
            }

            wstring IP;
            int port;
            cout << "Enter server IP address: ";
            wcin >> IP;
            cout << "Enter server port: ";
            cin >> port;

            sockaddr_in serverAddr;
            memset(&serverAddr, 0, sizeof(serverAddr));
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(port);
            InetPton(AF_INET, (IP.c_str()), &serverAddr.sin_addr);

            if (!clientSocket.Connect((SOCKADDR*)&serverAddr, sizeof(serverAddr)))
            {
                cerr << "Fail to connect to server.\n";
                return 1;
            }

            cout << "Connected to server.\n";

            int numFiles;
            clientSocket.Receive(&numFiles, sizeof(numFiles));

            vector<FileInfo> files(numFiles);

            for (int i = 0; i < numFiles; ++i)
            {
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
            for (const auto& file : files)
            {
                cout << file.name << " " << file.size << " Bytes\n";
            }

            for (const auto& file : files)
            {
                if (stop) break;
                DownloadFile(clientSocket, file.name);
            }

            clientSocket.Close();
        }
    }
    else
    {
        // Handle error in getting module handle
        wprintf(L"Fatal Error: GetModuleHandle failed\n");
        nRetCode = 1;
    }

    return nRetCode;
}
