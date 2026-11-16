#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <limits>
using namespace std;
namespace fs = std::filesystem;

const int MAX_FILES = 100;
const int MAX_QUEUE = 100;
const int MAX_UNDO = 500;

struct FileRecord {
    string fileName;
    string filePath;
    string status;
    string encryptedPath;
    uintmax_t fileSize;
};

struct HistoryNode {
    string operation;
    string fileName;
    HistoryNode* next;
};

struct UndoRecord {
    string operation;
    int fileIndex;
    string previousStatus;
    string outputPath;
};

struct QueueRecord {
    string fileName;
    string filePath;
};

FileRecord files[MAX_FILES];
int fileCount = 0;
HistoryNode* historyHead = NULL;
UndoRecord undoStack[MAX_UNDO];
int top = -1;
QueueRecord pendingQueue[MAX_QUEUE];
int front = -1;
int rear = -1;
int queueCount = 0;

void viewFiles();

bool pushUndo(string operation, int index, string previousStatus, string path) {
    if (top == MAX_UNDO - 1)
        return false;
    top++;
    undoStack[top].operation = operation;
    undoStack[top].fileIndex = index;
    undoStack[top].previousStatus = previousStatus;
    undoStack[top].outputPath = path;
    return true;
}

void addHistory(string operation, string name) {
    HistoryNode* newNode = new HistoryNode;
    newNode->operation = operation;
    newNode->fileName = name;
    newNode->next = NULL;
    if (historyHead == NULL) {
        historyHead = newNode;
    } else {
        HistoryNode* temp = historyHead;
        while (temp->next != NULL)
            temp = temp->next;
        temp->next = newNode;
    }
}

string uniquePath(string folder, string fileName, string extension) {
    fs::create_directories(folder);
    string baseName = fs::path(fileName).stem().string();
    string originalExtension = fs::path(fileName).extension().string();
    string suffix = extension;
    if (folder == "decrypted")
        suffix += originalExtension;
    string candidate = folder + "/" + baseName + suffix;
    int number = 1;
    while (fs::exists(candidate)) {
        candidate = folder + "/" + baseName + "_" + to_string(number) + suffix;
        number++;
    }
    return candidate;
}

bool copyToBackup(int index, string& destination) {
    destination = uniquePath("backup", files[index].fileName, "_copy");
    error_code ec;
    fs::copy_file(files[index].filePath, destination, ec);
    if (ec) {
        cout << "\nCopy failed: " << ec.message() << endl;
        return false;
    }
    return true;
}

void viewHistory() {
    if (historyHead == NULL) {
        cout << "\nNo operation history available!\n";
        return;
    }
    HistoryNode* temp = historyHead;
    cout << "\n------ OPERATION HISTORY ------\n";
    while (temp != NULL) {
        cout << "Operation: " << temp->operation;
        cout << " | File: " << temp->fileName << endl;
        temp = temp->next;
    }
}

void viewQueue() {
    if (queueCount == 0) {
        cout << "\nNo pending operations!\n";
        return;
    }
    cout << "\n------ PENDING COPY OPERATIONS ------\n";
    for (int i = 0; i < queueCount; i++) {
        int index = (front + i) % MAX_QUEUE;
        cout << i + 1 << ". Copy " << pendingQueue[index].fileName << endl;
    }
}

void undoOperation() {
    if (top == -1) {
        cout << "\nNothing to undo!\n";
        return;
    }
    UndoRecord action = undoStack[top];
    int i = action.fileIndex;
    error_code ec;

    if (action.operation == "ADD") {
        if (i < 0 || i >= fileCount) {
            cout << "\nUnable to undo registration because the file record is unavailable!\n";
            return;
        }
        string name = files[i].fileName;
        for (int j = i; j < fileCount - 1; j++)
            files[j] = files[j + 1];
        fileCount--;
        top--;
        for (int j = 0; j <= top; j++) {
            if (undoStack[j].fileIndex > i)
                undoStack[j].fileIndex--;
        }
        addHistory("Undo Add", name);
        cout << "\nFile registration undone!\n";
        return;
    }

    if (action.operation == "COPY" || action.operation == "ENCRYPT" || action.operation == "DECRYPT") {
        bool removed = fs::remove(action.outputPath, ec);
        if (ec || !removed) {
            cout << "\nUnable to undo operation. Output file may already be missing.\n";
            return;
        }
    }

    if (action.operation == "ENCRYPT") {
        files[i].encryptedPath = "";
        files[i].status = action.previousStatus;
        addHistory("Undo Encryption", files[i].fileName);
        cout << "\nEncryption undone!\n";
    } else if (action.operation == "DECRYPT") {
        files[i].status = action.previousStatus;
        addHistory("Undo Decryption", files[i].fileName);
        cout << "\nDecryption undone!\n";
    } else if (action.operation == "COPY") {
        addHistory("Undo Copy", files[i].fileName);
        cout << "\nFile copy undone!\n";
    } else {
        cout << "\nUnknown undo operation!\n";
        return;
    }
    top--;
}

void enqueueOperation() {
    if (queueCount == MAX_QUEUE) {
        cout << "\nQueue is full!\n";
        return;
    }
    if (fileCount == 0) {
        cout << "\nNo files available!\n";
        return;
    }
    viewFiles();
    int index;
    cout << "\nEnter file number to add to queue: ";
    if (!(cin >> index)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "\nInvalid input!\n";
        return;
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    if (index < 1 || index > fileCount) {
        cout << "\nInvalid file number!\n";
        return;
    }
    if (front == -1)
        front = 0;
    rear = (rear + 1) % MAX_QUEUE;
    pendingQueue[rear].fileName = files[index - 1].fileName;
    pendingQueue[rear].filePath = files[index - 1].filePath;
    queueCount++;
    cout << "\nCopy operation added to queue!\n";
}

void processQueue() {
    if (queueCount == 0) {
        cout << "\nQueue is empty!\n";
        return;
    }
    if (top == MAX_UNDO - 1) {
        cout << "\nUndo stack is full!\n";
        return;
    }
    QueueRecord task = pendingQueue[front];
    int fileIndex = -1;
    for (int i = 0; i < fileCount; i++) {
        if (files[i].filePath == task.filePath) {
            fileIndex = i;
            break;
        }
    }
    if (fileIndex == -1 || !fs::exists(task.filePath)) {
        cout << "\nQueued file is no longer available. Removing this queue item.\n";
    } else {
        string destination;
        if (!copyToBackup(fileIndex, destination))
            return;
        pushUndo("COPY", fileIndex, "", destination);
        addHistory("File Copied From Queue", task.fileName);
        cout << "\nOperation processed successfully!\n";
        cout << "File copied: " << task.fileName << endl;
    }
    front = (front + 1) % MAX_QUEUE;
    queueCount--;
    if (queueCount == 0)
        front = rear = -1;
}

void addFile() {
    if (fileCount >= MAX_FILES) {
        cout << "\nFile storage is full!\n";
        return;
    }
    if (top == MAX_UNDO - 1) {
        cout << "\nUndo stack is full!\n";
        return;
    }
    string path;
    cout << "\nEnter file path: ";
    getline(cin, path);
    if (!fs::exists(path) || !fs::is_regular_file(path)) {
        cout << "\nFile not found!\n";
        return;
    }
    files[fileCount].filePath = path;
    files[fileCount].fileName = fs::path(path).filename().string();
    files[fileCount].fileSize = fs::file_size(path);
    files[fileCount].status = "Registered";
    files[fileCount].encryptedPath = "";
    fileCount++;
    pushUndo("ADD", fileCount - 1, "", "");
    addHistory("File Added", files[fileCount - 1].fileName);
    cout << "\nFile added successfully!\n";
}

void viewFiles() {
    if (fileCount == 0) {
        cout << "\nNo files registered!\n";
        return;
    }
    cout << "\n------ REGISTERED FILES ------\n";
    for (int i = 0; i < fileCount; i++) {
        cout << "\n" << i + 1 << ". " << files[i].fileName;
        cout << "\nSize: " << files[i].fileSize << " bytes";
        cout << "\nStatus: " << files[i].status << endl;
    }
}

void readFile() {
    if (fileCount == 0) {
        cout << "\nNo files available!\n";
        return;
    }
    int index;
    viewFiles();
    cout << "\nEnter file number: ";
    if (!(cin >> index)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "\nInvalid input!\n";
        return;
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    if (index < 1 || index > fileCount) {
        cout << "\nInvalid file number!\n";
        return;
    }
    ifstream file(files[index - 1].filePath, ios::binary);
    if (!file) {
        cout << "\nUnable to open file!\n";
        return;
    }
    cout << "\n------ FILE CONTENTS ------\n";
    char data;
    while (file.get(data))
        cout.put(data);
    cout << endl;
}

void copyFile() {
    if (fileCount == 0) {
        cout << "\nNo files available!\n";
        return;
    }
    if (top == MAX_UNDO - 1) {
        cout << "\nUndo stack is full!\n";
        return;
    }
    int index;
    viewFiles();
    cout << "\nEnter file number: ";
    if (!(cin >> index)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "\nInvalid input!\n";
        return;
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    if (index < 1 || index > fileCount) {
        cout << "\nInvalid file number!\n";
        return;
    }
    string destination;
    if (!copyToBackup(index - 1, destination))
        return;
    pushUndo("COPY", index - 1, "", destination);
    addHistory("File Copied", files[index - 1].fileName);
    cout << "\nFile copied successfully!\n";
    cout << "Saved at: " << destination << endl;
}

void encryptFile() {
    if (fileCount == 0 || top == MAX_UNDO - 1) {
        cout << "\nNo files available or undo stack is full!\n";
        return;
    }
    int index;
    char key;
    viewFiles();
    cout << "\nEnter file number to encrypt: ";
    if (!(cin >> index)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "\nInvalid input!\n";
        return;
    }
    if (index < 1 || index > fileCount) {
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "\nInvalid file number!\n";
        return;
    }
    cout << "Enter encryption key (one character): ";
    cin >> key;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    int i = index - 1;
    string outputPath = uniquePath("encrypted", files[i].fileName, ".enc");
    ifstream input(files[i].filePath, ios::binary);
    if (!input) {
        cout << "\nUnable to open input file!\n";
        return;
    }
    ofstream output(outputPath, ios::binary);
    if (!output) {
        cout << "\nUnable to create encrypted file!\n";
        return;
    }
    char data;
    while (input.get(data))
        output.put(data ^ key);
    input.close();
    output.close();
    if (!output) {
        fs::remove(outputPath);
        cout << "\nEncryption failed!\n";
        return;
    }
    string previous = files[i].status;
    files[i].encryptedPath = outputPath;
    files[i].status = "Encrypted";
    pushUndo("ENCRYPT", i, previous, outputPath);
    addHistory("File Encrypted", files[i].fileName);
    cout << "\nFile encrypted successfully!\n";
    cout << "Saved at: " << outputPath << endl;
}

void decryptFile() {
    if (fileCount == 0 || top == MAX_UNDO - 1) {
        cout << "\nNo files available or undo stack is full!\n";
        return;
    }
    int index;
    char key;
    viewFiles();
    cout << "\nEnter file number to decrypt: ";
    if (!(cin >> index)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "\nInvalid input!\n";
        return;
    }
    if (index < 1 || index > fileCount) {
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "\nInvalid file number!\n";
        return;
    }
    int i = index - 1;
    if (files[i].encryptedPath.empty() || !fs::exists(files[i].encryptedPath)) {
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "\nPlease encrypt this file first!\n";
        return;
    }
    cout << "Enter decryption key (same character used for encryption): ";
    cin >> key;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    string outputPath = uniquePath("decrypted", files[i].fileName, "");
    ifstream input(files[i].encryptedPath, ios::binary);
    if (!input) {
        cout << "\nUnable to open encrypted file!\n";
        return;
    }
    ofstream output(outputPath, ios::binary);
    if (!output) {
        cout << "\nUnable to create decrypted file!\n";
        return;
    }
    char data;
    while (input.get(data))
        output.put(data ^ key);
    input.close();
    output.close();
    if (!output) {
        fs::remove(outputPath);
        cout << "\nDecryption failed!\n";
        return;
    }
    string previous = files[i].status;
    files[i].status = "Decrypted";
    pushUndo("DECRYPT", i, previous, outputPath);
    addHistory("File Decrypted", files[i].fileName);
    cout << "\nFile decrypted successfully!\n";
    cout << "Saved at: " << outputPath << endl;
}

void searchFile() {
    if (fileCount == 0) {
        cout << "\nNo files registered!\n";
        return;
    }
    string searchName;
    cout << "\nEnter filename to search: ";
    getline(cin, searchName);
    for (int i = 0; i < fileCount; i++) {
        if (files[i].fileName == searchName) {
            cout << "\nFile found!\n";
            cout << "File Name: " << files[i].fileName << endl;
            cout << "File Path: " << files[i].filePath << endl;
            cout << "File Size: " << files[i].fileSize << " bytes\n";
            cout << "Status: " << files[i].status << endl;
            return;
        }
    }
    cout << "\nFile not found!\n";
}

void sortFiles() {
    if (fileCount == 0) {
        cout << "\nNo files available to sort!\n";
        return;
    }
    for (int i = 0; i < fileCount - 1; i++) {
        for (int j = 0; j < fileCount - i - 1; j++) {
            if (files[j].fileName > files[j + 1].fileName) {
                swap(files[j], files[j + 1]);
                for (int k = 0; k <= top; k++) {
                    if (undoStack[k].fileIndex == j)
                        undoStack[k].fileIndex = j + 1;
                    else if (undoStack[k].fileIndex == j + 1)
                        undoStack[k].fileIndex = j;
                }
            }
        }
    }
    addHistory("Files Sorted", "All Files");
    cout << "\nFiles sorted alphabetically!\n";
    viewFiles();
}

bool compareFiles(string firstPath, string secondPath) {
    ifstream first(firstPath, ios::binary);
    ifstream second(secondPath, ios::binary);
    if (!first || !second)
        return false;
    char a, b;
    while (true) {
        bool readA = static_cast<bool>(first.get(a));
        bool readB = static_cast<bool>(second.get(b));
        if (readA != readB)
            return false;
        if (!readA)
            return true;
        if (a != b)
            return false;
    }
}

void verifyFile() {
    if (fileCount == 0) {
        cout << "\nNo files registered!\n";
        return;
    }
    int index;
    viewFiles();
    cout << "\nEnter file number to verify: ";
    if (!(cin >> index)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "\nInvalid input!\n";
        return;
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    if (index < 1 || index > fileCount) {
        cout << "\nInvalid file number!\n";
        return;
    }
    int i = index - 1;
    if (files[i].encryptedPath.empty()) {
        cout << "\nThis file has not been encrypted yet!\n";
        return;
    }
    string decryptedPath;
    cout << "Enter decrypted file path: ";
    getline(cin, decryptedPath);
    if (!fs::exists(decryptedPath) || !fs::is_regular_file(decryptedPath)) {
        cout << "\nDecrypted file not found!\n";
        return;
    }
    if (compareFiles(files[i].filePath, decryptedPath))
        cout << "\nVerification successful: Original and decrypted files are identical.\n";
    else
        cout << "\nVerification failed: Files are different.\n";
    addHistory("File Verification", files[i].fileName);
}

int main() {
    int choice;
    do {
        cout << "\n================================\n";
        cout << "   SECURE FILE MANAGEMENT SYSTEM\n";
        cout << "================================\n";
        cout << "1. Add File\n";
        cout << "2. View Files\n";
        cout << "3. Read File\n";
        cout << "4. Copy File\n";
        cout << "5. Encrypt File\n";
        cout << "6. Decrypt File\n";
        cout << "7. View Operation History\n";
        cout << "8. Undo Last Operation\n";
        cout << "9. Add Copy to Queue\n";
        cout << "10. View Pending Queue\n";
        cout << "11. Process Queue\n";
        cout << "12. Search File\n";
        cout << "13. Sort Files\n";
        cout << "14. Verify Decrypted File\n";
        cout << "15. Exit\n";
        cout << "\nEnter your choice: ";
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "\nInvalid input!\n";
            continue;
        }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        switch (choice) {
            case 1: addFile(); break;
            case 2: viewFiles(); break;
            case 3: readFile(); break;
            case 4: copyFile(); break;
            case 5: encryptFile(); break;
            case 6: decryptFile(); break;
            case 7: viewHistory(); break;
            case 8: undoOperation(); break;
            case 9: enqueueOperation(); break;
            case 10: viewQueue(); break;
            case 11: processQueue(); break;
            case 12: searchFile(); break;
            case 13: sortFiles(); break;
            case 14: verifyFile(); break;
            case 15: cout << "\nThank you for using the system!\n"; break;
            default: cout << "\nInvalid choice!\n";
        }
    } while (choice != 15);
    return 0;
}
