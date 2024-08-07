﻿#include "pch.h"
#include "framework.h"
#include <afxsock.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <windows.h>
#include <signal.h>
#include <filesystem>
#include <WS2tcpip.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CWinApp theApp;

using namespace std;

struct FileInfo
{
    string name = "";
    string size = "";
    string typeData = "";
};

bool processExchange = false;
bool failedOpenFile = false;
bool stop = false;

void signalHandler(int signum)
{
    cout << "\nInterrupt signal (" << signum << ") received. Exiting..." << endl;
    stop = true;
}

vector<FileInfo> receiveFiles(CSocket& client)
{
    int quantityFiles;
    client.Receive(&quantityFiles, sizeof(quantityFiles));

    vector<FileInfo> files;

    for (int i = 0; i < quantityFiles; ++i)
    {
        FileInfo temp;
        int length_name, length_size, length_typeData;

        client.Receive(&length_name, sizeof(length_name));
        char* temp_nameBuffer = new char[length_name + 1];
        client.Receive(temp_nameBuffer, length_name);
        temp_nameBuffer[length_name] = '\0';

        client.Receive(&length_size, sizeof(length_size));
        char* temp_sizeBuffer = new char[length_size + 1];
        client.Receive(temp_sizeBuffer, length_size);
        temp_sizeBuffer[length_size] = '\0';

        client.Receive(&length_typeData, sizeof(length_typeData));
        char* temp_typeDataBuffer = new char[length_typeData + 1];
        client.Receive(temp_typeDataBuffer, length_typeData);
        temp_typeDataBuffer[length_typeData] = '\0';

        temp.name = string(temp_nameBuffer);
        temp.size = string(temp_sizeBuffer);
        temp.typeData = string(temp_typeDataBuffer);

        files.push_back(temp);

        delete[] temp_nameBuffer;
        delete[] temp_sizeBuffer;
        delete[] temp_typeDataBuffer;
    }

    return files;
}

void writeFilesFromServer(string filename, vector<FileInfo> files)
{
    ofstream ofs;
    ofs.open(filename.c_str());

    if (!ofs.is_open())
    {
        cout << "Failed to opened file: " << filename << '\n';
        return;
    }

    for (int i = 0; i < files.size(); ++i)
    {
        ofs << files[i].name << " " << files[i].size << files[i].typeData << '\n';
    }
    ofs << "The lists of file downloaded:\n";

    ofs.close();
}

FileInfo getInfo(const string info)
{
    FileInfo temp;
    string temp_name;
    string temp_sizeAndtypeData;
    istringstream ss(info);

    ss >> temp_name >> temp_sizeAndtypeData;

    temp.name = temp_name;
    for (int i = 0; i < temp_sizeAndtypeData.size(); ++i)
    {
        if (temp_sizeAndtypeData[i] >= '0' && temp_sizeAndtypeData[i] <= '9')
        {
            temp.size += temp_sizeAndtypeData[i];
        }
        else
        {
            temp.typeData += temp_sizeAndtypeData[i];
        }
    }

    return temp;
}

vector<FileInfo> getListFiles(string filename)
{
    ifstream ifs;
    ifs.open(filename.c_str());

    if (!ifs.is_open())
    {
        cout << "Failed to opened file: " << filename << '\n';
        return {};
    }

    while (true)
    {
        string ignore;
        getline(ifs, ignore, '\n');
        if (ignore == "The lists of file downloaded:") break;
    }

    vector<FileInfo> files;
    while (!ifs.eof())
    {
        string line;
        getline(ifs, line, '\n');

        if (!line.empty())
        {
            FileInfo temp = getInfo(line);
            files.push_back(temp);
        }
    }

    ifs.close();

    return files;
}

vector<FileInfo> processFiles(CSocket& client)
{
    vector<FileInfo> filesFromServer = receiveFiles(client);
    writeFilesFromServer("input.txt", filesFromServer);

    int choice = 0;
    while (choice == 0)
    {
        system("cls");
        cout << "The list of file can downloaded:\n";
        for (int i = 0; i < filesFromServer.size(); ++i)
        {
            cout << filesFromServer[i].name << " " << filesFromServer[i].size << filesFromServer[i].typeData << '\n';
        }

        cout << "\nAfter you entered file named from the list file of server\n";
        cout << "Enter 1 if you have completed the file list and 0 if you have not completed it: ";
        cin >> choice;

        if (choice == 1) break;
    }

    vector<FileInfo> filesFromClient = getListFiles("input.txt");

    if (filesFromClient.empty())
    {
        int quantityFiles = -1;
        client.Send(&quantityFiles, sizeof(quantityFiles), 0);
    }
    else
    {
        int quantityFiles = filesFromClient.size();
        client.Send(&quantityFiles, sizeof(quantityFiles), 0);

        for (int i = 0; i < quantityFiles; ++i)
        {
            int length_name = filesFromClient[i].name.size();
            int length_size = filesFromClient[i].size.size();
            int length_typeData = filesFromClient[i].typeData.size();

            client.Send(&length_name, sizeof(length_name));
            client.Send(filesFromClient[i].name.c_str(), length_name);

            client.Send(&length_size, sizeof(length_size));
            client.Send(filesFromClient[i].size.c_str(), length_size);

            client.Send(&length_typeData, sizeof(length_typeData));
            client.Send(filesFromClient[i].typeData.c_str(), length_typeData);
        }
    }

    return filesFromClient;
}

void displayFilesSucess(vector<FileInfo> files)
{
    system("cls");
    cout << "Condition:\n";
    if (files.empty()) return;
    for (int i = 0; i < files.size(); ++i)
    {
        cout << files[i].name << ": download completed.\n";
    }
}

void downloadingFile(CSocket& client, FileInfo file, vector<FileInfo>& downloadCompeted)
{
    streamsize sizeBytes;
    client.Receive(&sizeBytes, sizeof(sizeBytes), 0);

    if (sizeBytes <= 0)
    {
        cout << "The package is not valid.\n";
        return;
    }

    string outputDir = "output//";
    if (!filesystem::exists(outputDir)) 
    {
        if (!filesystem::create_directory(outputDir)) 
        {
            cerr << "Error creating output directory." << endl;
            return;
        }
    }

    string path = outputDir + file.name;
    ofstream ofs;
    ofs.open(path.c_str(), ios::binary);

    if (ofs.is_open())
    {
        char packageReceive[1024];
        streamsize totalCurrentBytes = 0;

        cout << "\nRequest:\n";
        while (totalCurrentBytes < sizeBytes)
        {
            int bytesReceive = client.Receive(packageReceive, sizeof(packageReceive));
            totalCurrentBytes += bytesReceive;
            ofs.write(packageReceive, sizeof(packageReceive));

            int percentDownloading = totalCurrentBytes * 100 / sizeBytes;
            cout << "Downloading " << file.name << " .... " << percentDownloading << "%" << "\r";
            cout.flush();
        }

        ofs.close();
        cout << "Downloading " << file.name << " .... " << "100%" << "\r";
        downloadCompeted.push_back(file);
    }
    else
    {
        cout << "Failed to opened file: " << file.name << '\n';
        return;
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
            unsigned int port = 12345;
            cout << "Enter server IP address: ";
            cin >> IP;

            if (!clientSocket.Connect((CA2W)IP, port)) {
                cerr << "Fail to connect to server.\n";
                clientSocket.Close();
                system("pause");
                return 1;
            }

            system("cls");
            cout << "Connected to the server successfully.\n";
            vector<FileInfo> filesFromClient = processFiles(clientSocket);
            vector<FileInfo> display;

            for (int i = 0; i < filesFromClient.size(); ++i)
            {
                displayFilesSucess(display);
                downloadingFile(clientSocket, filesFromClient[i], display);
            }
            displayFilesSucess(display);

            cout << "\nAll files have been downloaded completely.\n";
            cout << "Close connection.\n";

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
