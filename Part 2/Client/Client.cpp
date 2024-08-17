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
#include <iostream>
#include <algorithm>

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

enum PriorityMode { NORMAL, HIGH, CRITICAL };

struct FileInfo
{
    string name = "";
    string size = "";
    string typeData = "";
    PriorityMode priority = NORMAL;
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
        cout << "Failed to open file: " << filename << '\n';
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
        cout << "Failed to open file: " << filename << '\n';
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

vector<FileInfo> scanNewFiles(const vector<FileInfo>& processedFiles) {
    vector<FileInfo> currentFiles = getListFiles("input.txt");
    vector<FileInfo> newFiles;

    for (const auto& file : currentFiles) {
        auto it = find_if(processedFiles.begin(), processedFiles.end(),
            [&](const FileInfo& processed) {
                return processed.name == file.name && processed.size == file.size && processed.typeData == file.typeData;
            });

        if (it == processedFiles.end()) {
            newFiles.push_back(file);
        }
    }

    return newFiles;
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

void displayDownloadProgress(const vector<FileInfo>& files, const vector<int>& percentComplete, int x, int y)
{
    for (size_t i = 0; i < files.size(); ++i)
    {
        gotoXY(x, y + i);
        cout << files[i].name << " - " << percentComplete[i] << "% complete    ";
    }
}
PriorityMode getPriorityModeFromString(const std::string& priorityStr) {
    if (priorityStr == "CRITICAL") return CRITICAL;
    if (priorityStr == "HIGH") return HIGH;
    return NORMAL;
}

int getChunksForPriority(PriorityMode priority) {
    switch (priority) {
    case CRITICAL: return 10;
    case HIGH: return 4;
    default: return 1;
    }
}

void downloadingFile(CSocket& client, std::vector<FileInfo>& files)
{
    if (files.empty())
    {
        std::cout << "The list of files is empty.\n";
        return;
    }

    std::string outputDir = "output//";
    if (!std::filesystem::exists(outputDir))
    {
        if (!std::filesystem::create_directory(outputDir))
        {
            std::cerr << "Error creating output directory." << std::endl;
            return;
        }
    }

    std::vector<std::string> filesname;
    for (const auto& file : files)
    {
        filesname.push_back(outputDir + file.name);
    }

    std::vector<std::ofstream> outFileStreams(files.size());
    std::vector<streamsize> fileSizes(files.size());
    std::vector<streamsize> bytesReceived(files.size(), 0);
    std::vector<bool> fileDownloaded(files.size(), false);
    std::vector<int> percentComplete(files.size(), 0);

    // Receiving file sizes from the server
    for (size_t i = 0; i < files.size(); ++i)
    {
        client.Receive(&fileSizes[i], sizeof(fileSizes[i]));
        std::cout << "File: " << files[i].name << " - Size: " << fileSizes[i] << " bytes\n";

        if (fileSizes[i] <= 0)
        {
            std::cerr << "Failed to receive the size of file: " << files[i].name << '\n';
            return;
        }

        outFileStreams[i].open(filesname[i], std::ios::binary);
        if (!outFileStreams[i].is_open())
        {
            std::cerr << "Failed to open file: " << files[i].name << '\n';
            return;
        }
    }

    char buffer[1024];
    system("cls");
    int x = 0, y = 0; // Initial position for displaying the download progress

    while (true)
    {
        bool allFilesDownloaded = std::all_of(fileDownloaded.begin(), fileDownloaded.end(), [](bool downloaded) { return downloaded; });
        if (allFilesDownloaded) break;

        for (size_t i = 0; i < files.size(); ++i)
        {
            if (fileDownloaded[i]) continue;

            int chunksToDownload = getChunksForPriority(files[i].priority);
            for (int chunk = 0; chunk < chunksToDownload && !fileDownloaded[i]; ++chunk)
            {
                streamsize bytesToReceive = min(static_cast<streamsize>(sizeof(buffer)), fileSizes[i] - bytesReceived[i]);
                int bytesReceivedThisChunk = client.Receive(buffer, bytesToReceive);

                if (bytesReceivedThisChunk > 0)
                {
                    bytesReceived[i] += bytesReceivedThisChunk;
                    outFileStreams[i].write(buffer, bytesReceivedThisChunk);
                    percentComplete[i] = static_cast<int>((bytesReceived[i] * 100.0) / fileSizes[i]);

                    // Display the download progress for the current state
                    displayDownloadProgress(files, percentComplete, x, y);

                    if (bytesReceived[i] >= fileSizes[i])
                    {
                        fileDownloaded[i] = true;
                        std::cout << "\nDownload completed: " << files[i].name << "\n";
                    }
                }
                else
                {
                    std::cerr << "Error receiving file data.\n";
                    return;
                }
            }
        }
    }

    for (auto& file : outFileStreams)
    {
        if (file.is_open())
        {
            file.close();
        }
    }

    std::cout << "\nAll files have been successfully downloaded.\n";
}

std::vector<FileInfo> processFiles(CSocket& client) {
    std::vector<FileInfo> filesFromServer = receiveFiles(client);
    writeFilesFromServer("input.txt", filesFromServer);

    int choice = 0;
    while (choice == 0) {
        system("cls");
        std::cout << "The list of files that can be downloaded:\n";
        for (int i = 0; i < filesFromServer.size(); ++i) {
            std::cout << filesFromServer[i].name << " " << filesFromServer[i].size << filesFromServer[i].typeData << '\n';
        }

        std::cout << "\nAfter you have entered file names from the server's list of files\n";
        std::cout << "Enter 1 if you have completed the file list and 0 if you have not completed it: ";
        std::cin >> choice;

        if (choice == 1) break;
    }

    std::vector<FileInfo> filesFromClient = getListFiles("input.txt");

    if (filesFromClient.empty()) {
        int quantityFiles = -1;
        client.Send(&quantityFiles, sizeof(quantityFiles), 0);
    }
    else {
        // Assign priority to each file based on input
        for (auto& file : filesFromClient) {
            std::cout << "Assign priority (NORMAL, HIGH, CRITICAL) for " << file.name << ": ";
            std::string priority;
            std::cin >> priority;
            file.priority = getPriorityModeFromString(priority);
        }

        int quantityFiles = filesFromClient.size();
        client.Send(&quantityFiles, sizeof(quantityFiles), 0);

        for (int i = 0; i < quantityFiles; ++i) {
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

            vector<FileInfo> processedFiles;

            // Bước đầu tiên: tải danh sách file từ server ngay sau khi kết nối
            vector<FileInfo> filesFromServer = processFiles(clientSocket);
            if (!filesFromServer.empty()) {
                downloadingFile(clientSocket, filesFromServer);
                processedFiles.insert(processedFiles.end(), filesFromServer.begin(), filesFromServer.end());
            }

            // Vòng lặp quét định kỳ
            while (!stop) {
                vector<FileInfo> newFiles = scanNewFiles(processedFiles);

                if (!newFiles.empty()) {
                    // Gửi các file mới để tải về
                    int quantityFiles = newFiles.size();
                    clientSocket.Send(&quantityFiles, sizeof(quantityFiles), 0);

                    for (const auto& file : newFiles) {
                        int length_name = file.name.size();
                        int length_size = file.size.size();
                        int length_typeData = file.typeData.size();

                        clientSocket.Send(&length_name, sizeof(length_name));
                        clientSocket.Send(file.name.c_str(), length_name);

                        clientSocket.Send(&length_size, sizeof(length_size));
                        clientSocket.Send(file.size.c_str(), length_size);

                        clientSocket.Send(&length_typeData, sizeof(length_typeData));
                        clientSocket.Send(file.typeData.c_str(), length_typeData);
                    }

                    // Tải về các file mới từ server
                    downloadingFile(clientSocket, newFiles);

                    // Cập nhật danh sách các file đã xử lý
                    processedFiles.insert(processedFiles.end(), newFiles.begin(), newFiles.end());
                }

                Sleep(2000); // Chờ 2 giây trước khi quét lại
            }

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
