#include "pch.h"
#include "framework.h"
#include "Server.h"
#include <afxsock.h>
#include <vector>
#include <windows.h>
#include <string>
#include <sstream>
#include <fstream>
#include <cmath>

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

vector<FileInfo> getListFiles(const string filename)
{
    ifstream ifs;
    ifs.open(filename.c_str());

    if (!ifs.is_open())
    {
        cout << "Failed to open file.\n";
        return {};
    }

    vector<FileInfo> list;
    while (!ifs.eof())
    {
        string line;
        getline(ifs, line, '\n');
        if (!line.empty())
        {
            FileInfo temp = getInfo(line);
            list.push_back(temp);
        }
    }

    ifs.close();

    return list;
}

void sendFiles(CSocket* pClient, const vector<FileInfo> files)
{
    int quantityFiles = files.size();
    pClient->Send(&quantityFiles, sizeof(quantityFiles));

    for (int i = 0; i < quantityFiles; ++i)
    {
        int length_name = files[i].name.size();
        int length_size = files[i].size.size();
        int length_typeData = files[i].typeData.size();

        pClient->Send(&length_name, sizeof(length_name));
        pClient->Send(files[i].name.c_str(), length_name);

        pClient->Send(&length_size, sizeof(length_size));
        pClient->Send(files[i].size.c_str(), length_size);

        pClient->Send(&length_typeData, sizeof(length_typeData));
        pClient->Send(files[i].typeData.c_str(), length_typeData);
    }
}

vector<FileInfo> receiveFiles(CSocket* pClient)
{
    int quantityFiles;
    pClient->Receive(&quantityFiles, sizeof(quantityFiles), 0);

    if (quantityFiles == -1)
    {
        cout << "Could not receive files from client.\n";
        return {};
    }

    vector<FileInfo> files;
    FileInfo temp;
    for (int i = 0; i < quantityFiles; ++i)
    {
        int length_name, length_size, length_typeData;

        pClient->Receive(&length_name, sizeof(length_name), 0);
        char* temp_nameBuffer = new char[length_name + 1];
        pClient->Receive(temp_nameBuffer, length_name, 0);
        temp_nameBuffer[length_name] = '\0';

        pClient->Receive(&length_size, sizeof(length_size), 0);
        char* temp_sizeBuffer = new char[length_size + 1];
        pClient->Receive(temp_sizeBuffer, length_size, 0);
        temp_sizeBuffer[length_size] = '\0';

        pClient->Receive(&length_typeData, sizeof(length_typeData), 0);
        char* temp_typeDataBuffer = new char[length_typeData + 1];
        pClient->Receive(temp_typeDataBuffer, length_typeData, 0);
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

void handleClient(CSocket* pClient)
{
    vector<FileInfo> filesFromClient = receiveFiles(pClient);

    if (!filesFromClient.empty())
    {
        cout << "\nClient wants to download the following files:\n";
        for (int i = 0; i < filesFromClient.size(); ++i)
        {
            cout << i + 1 << ". " << filesFromClient[i].name << " " << filesFromClient[i].size << filesFromClient[i].typeData << '\n';
        }
    }
    else
    {
        cout << "The text file containing the names of downloading files is empty!\n";
        return;
    }

    string Dir = "File\\";
    vector<string> filesname;
    for (int i = 0; i < filesFromClient.size(); ++i)
    {
        string path = Dir + filesFromClient[i].name;
        filesname.push_back(path);
    }

    vector<ifstream> files(filesFromClient.size());
    vector<streamsize> sizeOfFile(filesFromClient.size(), 0);
    vector<bool> sendAllFile(filesFromClient.size(), false);

    for (int i = 0; i < filesFromClient.size(); ++i)
    {
        files[i].open(filesname[i], ios::binary);

        if (!files[i].is_open())
        {
            cout << "Failed to open file: " << filesFromClient[i].name << '\n';
            streamsize Error = -1;
            pClient->Send(&Error, sizeof(Error));
            return;
        }

        files[i].seekg(0, ios::end);
        sizeOfFile[i] = files[i].tellg();
        pClient->Send(&sizeOfFile[i], sizeof(sizeOfFile[i]));
        cout << "Sent file size: " << sizeOfFile[i] << " for file " << filesFromClient[i].name << '\n'; // Ghi log
        files[i].seekg(0, ios::beg);
    }

    char packageSend[1024];
    while (true)
    {
        bool isSendAll = true;
        for (int i = 0; i < filesFromClient.size(); ++i)
        {
            if (!sendAllFile[i])
            {
                isSendAll = false;
                break;
            }
        }

        if (isSendAll) break;

        for (int i = 0; i < filesFromClient.size(); ++i)
        {
            if (sizeOfFile[i] > 0 && !sendAllFile[i])
            {
                int bytesToSend = min((streamsize)sizeof(packageSend), sizeOfFile[i]);
                files[i].read(packageSend, bytesToSend);
                pClient->Send((char*)&i, sizeof(i), 0);
                int bytesSent = pClient->Send(packageSend, bytesToSend);
                sizeOfFile[i] -= bytesSent;

                cout << "Sent " << bytesSent << " bytes for file " << filesFromClient[i].name << " remaining: " << sizeOfFile[i] << '\n'; // Ghi log

                if (sizeOfFile[i] <= 0) {
                    sendAllFile[i] = true;
                }
            }
        }
    }

    for (int i = 0; i < filesFromClient.size(); ++i)
    {
        files[i].close();
    }
}

DWORD WINAPI processClient(LPVOID arg)
{
    SOCKET* hConnected = (SOCKET*)arg;
    CSocket* pClient = new CSocket;
    pClient->Attach(*hConnected);

    vector<FileInfo> listFilesSend = getListFiles("input.txt");
    sendFiles(pClient, listFilesSend);
    handleClient(pClient);

    pClient->Close();

    cout << "\nClient closed the connection.\n";

    delete hConnected;
    delete pClient;
    return 0;
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

            DWORD threadID;
            HANDLE threadStatus;

            cout << "Server is listening on port 12345.\n\n";

            while (true)
            {
                CSocket* pClient = new CSocket();
                if (serverSocket.Accept(*pClient))
                {
                    cout << "\nClient connected.\n";
                    SOCKET* hConnected = new SOCKET;
                    *hConnected = pClient->Detach();
                    threadStatus = CreateThread(NULL, 0, processClient, hConnected, 0, &threadID);
                }
                else
                {
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
