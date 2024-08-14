#include "pch.h"
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

void gotoXY(int x, int y) 
{
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

struct FileInfo
{
    string name = "";
    string size = "";
    string typeData = "";
};

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

void displayPercentageOfDownload(int x, int y, vector<FileInfo> listFilesHandle, vector<int> percent, vector<bool> receiveFile, int mode)
{
    if (mode == 0)
    {
        system("cls");
        cout << "Request:\n";
        for (int i = 0; i < listFilesHandle.size(); ++i)
        {
            if (!receiveFile[i])
            {
                gotoXY(x, y++);
                cout << "Downloading " << listFilesHandle[i].name << " . . . . " << percent[i] << "%";
            }
        }
        gotoXY(x, y++);
        cout << "Condition:\n";
        for (int i = 0; i < listFilesHandle.size(); ++i)
        {
            if (receiveFile[i])
            {
                gotoXY(x, y++);
                cout << listFilesHandle[i].name << ": download completed.\n";
            }
        }
    }
    else if (mode == 1)
    {
        for (int i = 0; i < listFilesHandle.size(); ++i)
        {
            if (!receiveFile[i])
            {
                gotoXY(x, y++);
                cout << "Downloading " << listFilesHandle[i].name << " . . . . " << percent[i] << "%";
            }
        }
    }
    else if (mode == 2)
    {
        system("cls");
        gotoXY(x, y++);
        cout << "Condition:\n";
        for (int i = 0; i < listFilesHandle.size(); ++i)
        {
            if (receiveFile[i])
            {
                gotoXY(x, y++);
                cout << listFilesHandle[i].name << ": download completed.\n";
            }
        }
        
        cout << "\nAll files have been downloaded completely.\n";
    }
}

void downloadingFile(CSocket& client, vector<FileInfo> listFilesHandle)
{
    if (listFilesHandle.empty())
    {
        cout << "The list file is empty.\n";
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

    vector<string> filesname;
    for (int i = 0; i < listFilesHandle.size(); ++i)
    {
        string path = outputDir + listFilesHandle[i].name;
        filesname.push_back(path);
    }

    vector<ofstream> files(listFilesHandle.size());
    vector<streamsize> sizeOfFile(listFilesHandle.size());
    vector<streamsize> totalBytesReceive(listFilesHandle.size(), 0);
    vector<bool> receiveFile(listFilesHandle.size(), false);
    vector<int> percent(listFilesHandle.size(), 0);

    for (int i = 0; i < listFilesHandle.size(); ++i)
    {
        client.Receive(&sizeOfFile[i], sizeof(sizeOfFile[i]));
        if (sizeOfFile[i] <= 0)
        {
            cout << sizeOfFile[i] << '\n';
            cout << "Failed to receive the bytes of file: " << listFilesHandle[i].name << '\n';
            return;
        }
    }

    for (int i = 0; i < listFilesHandle.size(); ++i)
    {
        files[i].open(filesname[i], ios::binary);

        if (!files[i].is_open())
        {
            cout << "Failed to opened file: " << filesname[i] << '\n';
            return;
        }
    }

    char packageReceive[1024];
    system("cls");
    cout << "Request:\n";
    while (true)
    {
        bool isReceiveAll = true;
        for (int i = 0; i < listFilesHandle.size(); ++i)
        {
            if (!receiveFile[i])
            {
                isReceiveAll = false;
                break;
            }
        }

        if (isReceiveAll) break;

        int idxFile;
        int x = 0, y = 1;
        client.Receive((char*)&idxFile, sizeof(int), 0);
        if (idxFile >= 0 && idxFile < listFilesHandle.size())
        {
            int bytesReceive = client.Receive(packageReceive, sizeof(packageReceive));
            totalBytesReceive[idxFile] += bytesReceive;
            files[idxFile].write(packageReceive, sizeof(packageReceive));
            percent[idxFile] = totalBytesReceive[idxFile] * 100.0 / sizeOfFile[idxFile];
            displayPercentageOfDownload(0, 1, listFilesHandle, percent, receiveFile, 1);
            if (totalBytesReceive[idxFile] >= sizeOfFile[idxFile])
            {
                receiveFile[idxFile] = true;
                displayPercentageOfDownload(0, 1, listFilesHandle, percent, receiveFile, 0);
            }
        }
        else
        {
            cout << "Error to send index.\n";
            return;
        }
    }

    for (int i = 0; i < listFilesHandle.size(); ++i)
    {
        files[i].close();
    }

    displayPercentageOfDownload(0, 0, listFilesHandle, percent, receiveFile, 2);
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
            downloadingFile(clientSocket, filesFromClient);

            cout << "\nClose connection.\n";

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
