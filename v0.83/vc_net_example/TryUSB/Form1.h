#pragma once


namespace TryUSB
{
	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;
	using namespace System::Threading;
	using namespace System::Text;
	using namespace System::Globalization;

	const UInt32 FT_LIST_NUMBER_ONLY = 0x80000000;
	const UInt32 FT_LIST_BY_INDEX	= 0x40000000;
	const UInt32 FT_LIST_ALL			= 0x20000000;
	const UInt32 FT_OPEN_BY_SERIAL_NUMBER = 1;
	const UInt32 FT_OPEN_BY_DESCRIPTION    = 2;

	const UInt32 FT_EVENT_RXCHAR		= 1;
	const UInt32 FT_EVENT_MODEM_STATUS  = 2;

#define FT_PREFIX [DllImport("FTD2XX.dll")]

enum {
    FT_OK,
    FT_INVALID_HANDLE,
    FT_DEVICE_NOT_FOUND,
    FT_DEVICE_NOT_OPENED,
    FT_IO_ERROR,
    FT_INSUFFICIENT_RESOURCES,
    FT_INVALID_PARAMETER,
    FT_INVALID_BAUD_RATE,

    FT_DEVICE_NOT_OPENED_FOR_ERASE,
    FT_DEVICE_NOT_OPENED_FOR_WRITE,
    FT_FAILED_TO_WRITE_DEVICE,
    FT_EEPROM_READ_FAILED,
    FT_EEPROM_WRITE_FAILED,
    FT_EEPROM_ERASE_FAILED,
	FT_EEPROM_NOT_PRESENT,
	FT_EEPROM_NOT_PROGRAMMED,
	FT_INVALID_ARGS,
	FT_NOT_SUPPORTED,
	FT_OTHER_ERROR
};

	typedef void * FT_HANDLE;
	typedef unsigned long DWORD;
	typedef unsigned long FT_STATUS;
	typedef void * LPVOID;
	typedef void * PVOID;
	typedef DWORD * LPDWORD;
	typedef DWORD ULONG;
	typedef unsigned short USHORT;
	typedef unsigned char UCHAR;
	typedef unsigned short WORD;
	typedef WORD * LPWORD;
	typedef unsigned char UCHAR;
	typedef UCHAR * PUCHAR;
	typedef char CHAR;
	typedef CHAR * PCHAR;
	typedef ULONG FT_DEVICE;
	typedef void *HANDLE;
	typedef int BOOL;
	#define FALSE   0
	#define TRUE    1
	// as c++. net is a managed application and our ftd2xx.dll is unmanaged code you must declare the functions here explicitly
	// to allow you to call them within the application. An include file and the .lib file simply wont work with c++.net. Its
	// a similar problem in c#.
	// see http://www.codeguru.com/Cpp/COM-Tech/complus/managed/article.php/c3947/ for more on this.
	// I have only included these 4 functions in this to show you how to do this. For other functions that you require similar
	// declerations will need to go here. 
 
	using namespace System::Runtime::InteropServices;
	FT_PREFIX FT_STATUS FT_Open(int deviceNumber, FT_HANDLE * pHandle);
	FT_PREFIX FT_STATUS FT_OpenEx(PVOID pArg1, DWORD Flags, FT_HANDLE *pHandle);
	FT_PREFIX FT_STATUS FT_ListDevices(PVOID pArg1, PVOID pArg2, DWORD Flags);
	FT_PREFIX FT_STATUS FT_ListDevices(UInt32 pvArg1, void * pvArg2, UInt32 dwFlags);	// FT_ListDevcies by serial number or description by index only
	FT_PREFIX FT_STATUS FT_Close(FT_HANDLE ftHandle);
	FT_PREFIX FT_STATUS FT_Read(FT_HANDLE ftHandle, LPVOID lpBuffer, DWORD nBufferSize, LPDWORD lpBytesReturned);
	FT_PREFIX FT_STATUS FT_Write(FT_HANDLE ftHandle, LPVOID lpBuffer, DWORD nBufferSize, LPDWORD lpBytesWritten);
	FT_PREFIX FT_STATUS FT_SetBaudRate(FT_HANDLE ftHandle,	ULONG BaudRate);
	FT_PREFIX FT_STATUS FT_SetDivisor(FT_HANDLE ftHandle,USHORT Divisor);
	FT_PREFIX FT_STATUS FT_SetDataCharacteristics(FT_HANDLE ftHandle, UCHAR WordLength, UCHAR StopBits, UCHAR Parity);
	FT_PREFIX FT_STATUS FT_SetFlowControl(FT_HANDLE ftHandle, USHORT FlowControl, UCHAR XonChar, UCHAR XoffChar	);
	FT_PREFIX FT_STATUS FT_ResetDevice(FT_HANDLE ftHandle);
	FT_PREFIX FT_STATUS FT_SetDtr(FT_HANDLE ftHandle);
	FT_PREFIX FT_STATUS FT_ClrDtr(FT_HANDLE ftHandle);
	FT_PREFIX FT_STATUS FT_SetRts(FT_HANDLE ftHandle);
	FT_PREFIX FT_STATUS FT_ClrRts(FT_HANDLE ftHandle);
	FT_PREFIX FT_STATUS FT_GetModemStatus(FT_HANDLE ftHandle,	ULONG *pModemStatus);
	FT_PREFIX FT_STATUS FT_SetChars(FT_HANDLE ftHandle, UCHAR EventChar, UCHAR EventCharEnabled, UCHAR ErrorChar, UCHAR ErrorCharEnabled);
	FT_PREFIX FT_STATUS FT_Purge(FT_HANDLE ftHandle, ULONG Mask);
	FT_PREFIX FT_STATUS FT_SetTimeouts(FT_HANDLE ftHandle,	ULONG ReadTimeout,	ULONG WriteTimeout);
	FT_PREFIX FT_STATUS FT_GetQueueStatus(FT_HANDLE ftHandle, DWORD *dwRxBytes);
	FT_PREFIX FT_STATUS FT_SetEventNotification(FT_HANDLE ftHandle, DWORD Mask,	PVOID Param	);
	FT_PREFIX FT_STATUS FT_GetStatus(FT_HANDLE ftHandle, DWORD *dwRxBytes, DWORD *dwTxBytes, DWORD *dwEventDWord);
	FT_PREFIX FT_STATUS FT_SetBreakOn(FT_HANDLE ftHandle);
	FT_PREFIX FT_STATUS FT_SetBreakOff(FT_HANDLE ftHandle);	
	FT_PREFIX FT_STATUS FT_SetWaitMask(FT_HANDLE ftHandle, DWORD Mask);
	FT_PREFIX FT_STATUS FT_WaitOnMask(FT_HANDLE ftHandle, DWORD *Mask);
	FT_PREFIX FT_STATUS FT_GetEventStatus(FT_HANDLE ftHandle, DWORD *dwEventDWord);
	FT_PREFIX FT_STATUS FT_ReadEE(FT_HANDLE ftHandle, DWORD dwWordOffset, LPWORD lpwValu);
	FT_PREFIX FT_STATUS FT_WriteEE(FT_HANDLE ftHandle,	DWORD dwWordOffset, WORD wValue);
	FT_PREFIX FT_STATUS FT_EraseEE(FT_HANDLE ftHandle);
// Missed out the programming stuff +++ 

	FT_PREFIX FT_STATUS  FT_EE_UASize(FT_HANDLE ftHandle, LPDWORD lpdwSize);
	FT_PREFIX FT_STATUS  FT_EE_UAWrite(FT_HANDLE ftHandle, PUCHAR pucData, DWORD dwDataLen	);
	FT_PREFIX FT_STATUS  FT_EE_UARead(FT_HANDLE ftHandle, PUCHAR pucData, DWORD dwDataLen, LPDWORD lpdwBytesRead);
	FT_PREFIX FT_STATUS  FT_SetLatencyTimer(FT_HANDLE ftHandle, UCHAR ucLatency);
	FT_PREFIX FT_STATUS  FT_GetLatencyTimer(FT_HANDLE ftHandle, PUCHAR pucLatency);
	FT_PREFIX FT_STATUS  FT_SetBitMode(FT_HANDLE ftHandle, UCHAR ucMask, UCHAR ucEnable);
	FT_PREFIX FT_STATUS  FT_GetBitMode(FT_HANDLE ftHandle, PUCHAR pucMode);
	FT_PREFIX FT_STATUS  FT_SetUSBParameters(FT_HANDLE ftHandle, ULONG ulInTransferSize, ULONG ulOutTransferSize);
	FT_PREFIX FT_STATUS  FT_GetDeviceInfo(FT_HANDLE ftHandle, FT_DEVICE *lpftDevice, LPDWORD lpdwID, PCHAR SerialNumber, PCHAR Description,	LPVOID Dummy);
	FT_PREFIX FT_STATUS  FT_StopInTask(FT_HANDLE ftHandle);
	FT_PREFIX FT_STATUS  FT_RestartInTask(FT_HANDLE ftHandle);
	FT_PREFIX FT_STATUS  FT_SetResetPipeRetryCount(FT_HANDLE ftHandle, DWORD dwCount);
	FT_PREFIX FT_STATUS  FT_ResetPort(FT_HANDLE ftHandle);

	// need these kernel functions for the Event Handling stuff
	[DllImport("Kernel32.dll")] DWORD WaitForSingleObject(HANDLE hHandle,  DWORD dwMilliseconds);
	[DllImport("Kernel32.dll")] HANDLE CreateEvent(void * pNULL, BOOL bManualReset, BOOL bInitialState, char * pcNULL);
	[DllImport("Kernel32.dll")] BOOL SetEvent(HANDLE hEvent);
// missed out the W32 stuff as well

	FT_HANDLE handle;
	HANDLE hEvent;
	/// <summary> 
	/// Summary for Form1
	///
	/// WARNING: If you change the name of this class, you will need to change the 
	///          'Resource File Name' property for the managed resource compiler tool 
	///          associated with all .resx files this class depends on.  Otherwise,
	///          the designers will not be able to interact properly with localized
	///          resources associated with this form.
	/// </summary>
	public __gc class Form1 : public System::Windows::Forms::Form
	{	
	public:
		Form1(void)
		{
			InitializeComponent();
		}

	private:
		void ReadingProc()
        {
			ArrayList * palReadData;
			palReadData = new ArrayList();
			TimeSpan waitTime = TimeSpan(0, 0, 1);	// 1 second timeout
			static char c = 0;
			char* pstr6 = &c;
			FT_STATUS status;

			while(bContinue) {
				DWORD dwRead, dwRXBytes;
				Byte b;
				NumberFormatInfo*   provider = new NumberFormatInfo( );

				WaitForSingleObject(hEvent, -1);
				if(handle) {
					status = FT_GetQueueStatus(handle, &dwRead);
					if(status != FT_OK) {
						MessageBox::Show("GError");
						continue;
					}
					while(dwRead && bContinue) {
						status = FT_Read(handle, &b, 1, &dwRXBytes);
						if(status != FT_OK) {
							MessageBox::Show("RError");
							continue;
						}
						else {
							String* s;
							s = Convert::ToString(b);
							listBox1->Items->Add(s);
						}
						status = FT_GetQueueStatus(handle, &dwRead);
					}
				}
				Thread::Sleep(0);
			}
			if(handle) {
				FT_Close(handle);
			}
        }
  
	protected:
		void Dispose(Boolean disposing)
		{
			StopThread();		// Close our thread

			if (disposing && components)
			{
				components->Dispose();
			}
			__super::Dispose(disposing);
		}

	private: System::Windows::Forms::Button *  btnClose;
	private: System::Windows::Forms::Button *  btnSend;
	private: System::Threading::Thread * thrdReader;

	private: System::Windows::Forms::ListBox *  listBox1;
	private: System::Windows::Forms::ComboBox * comboBox1;
	private: System::Windows::Forms::GroupBox *  groupBox1;
	private: System::Windows::Forms::RadioButton *  radioNumber;
	private: System::Windows::Forms::RadioButton *  radioDescription;
	private: System::Windows::Forms::RadioButton *  radioSerial;
	private: bool bContinue;
	private: UInt32 dwOpenFlags;
	private: ManualResetEvent * ev;
	private: System::Windows::Forms::Label *  label1;

	private:
		/// <summary>
		/// Required designer variable.
		/// </summary>
		System::ComponentModel::Container * components;

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		void InitializeComponent(void)
		{
			this->btnClose = new System::Windows::Forms::Button();
			this->btnSend = new System::Windows::Forms::Button();
			this->listBox1 = new System::Windows::Forms::ListBox();
			this->comboBox1 = new System::Windows::Forms::ComboBox();
			this->radioNumber = new System::Windows::Forms::RadioButton();
			this->radioDescription = new System::Windows::Forms::RadioButton();
			this->radioSerial = new System::Windows::Forms::RadioButton();
			this->groupBox1 = new System::Windows::Forms::GroupBox();
			this->label1 = new System::Windows::Forms::Label();
			this->SuspendLayout();
			// 
			// btnClose
			// 
			this->btnClose->Location = System::Drawing::Point(208, 280);
			this->btnClose->Name = S"btnClose";
			this->btnClose->TabIndex = 1;
			this->btnClose->Text = S"Exit";
			this->btnClose->Click += new System::EventHandler(this, ButtonCancel_Click);
			// 
			// btnSend
			// 
			this->btnSend->Location = System::Drawing::Point(208, 192);
			this->btnSend->Name = S"btnSend";
			this->btnSend->TabIndex = 2;
			this->btnSend->Text = S"Write";
			this->btnSend->Click += new System::EventHandler(this, btnSend_Click);
			// 
			// listBox1
			// 
			this->listBox1->Location = System::Drawing::Point(160, 24);
			this->listBox1->Name = S"listBox1";
			this->listBox1->Size = System::Drawing::Size(120, 134);
			this->listBox1->TabIndex = 3;
			// 
			// comboBox1
			// 
			this->comboBox1->Location = System::Drawing::Point(16, 24);
			this->comboBox1->Name = S"comboBox1";
			this->comboBox1->Size = System::Drawing::Size(121, 21);
			this->comboBox1->TabIndex = 4;
			this->comboBox1->SelectedIndexChanged += new System::EventHandler(this, comboBox1_SelectedIndexChanged);
			// 
			// radioNumber
			// 
			this->radioNumber->Location = System::Drawing::Point(24, 88);
			this->radioNumber->Name = S"radioNumber";
			this->radioNumber->TabIndex = 5;
			this->radioNumber->Text = S"Number";
			this->radioNumber->CheckedChanged += new System::EventHandler(this, radioNumber_CheckedChanged);
			// 
			// radioDescription
			// 
			this->radioDescription->Location = System::Drawing::Point(24, 112);
			this->radioDescription->Name = S"radioDescription";
			this->radioDescription->TabIndex = 6;
			this->radioDescription->Text = S"Description";
			this->radioDescription->CheckedChanged += new System::EventHandler(this, radioDescription_CheckedChanged);
			// 
			// radioSerial
			// 
			this->radioSerial->Location = System::Drawing::Point(24, 136);
			this->radioSerial->Name = S"radioSerial";
			this->radioSerial->TabIndex = 7;
			this->radioSerial->Text = S"Serial";
			this->radioSerial->CheckedChanged += new System::EventHandler(this, radioSerial_CheckedChanged);
			// 
			// groupBox1
			// 
			this->groupBox1->Location = System::Drawing::Point(8, 72);
			this->groupBox1->Name = S"groupBox1";
			this->groupBox1->Size = System::Drawing::Size(128, 96);
			this->groupBox1->TabIndex = 8;
			this->groupBox1->TabStop = false;
			this->groupBox1->Text = S"List Devices By";
			// 
			// label1
			// 
			this->label1->Location = System::Drawing::Point(16, 192);
			this->label1->Name = S"label1";
			this->label1->Size = System::Drawing::Size(176, 112);
			this->label1->TabIndex = 9;
			this->label1->Text = S"Sample to perform a simple write/read on an FTDI device using the D2XX driver. As" 
				S"sumes there is a working FTDI device with a suitable loopback connector on the c" 
				S"able (see www.easysync.co.uk for compatible cables)";
			// 
			// Form1
			// 
			this->AutoScaleBaseSize = System::Drawing::Size(5, 13);
			this->ClientSize = System::Drawing::Size(292, 316);
			this->Controls->Add(this->label1);
			this->Controls->Add(this->radioSerial);
			this->Controls->Add(this->radioDescription);
			this->Controls->Add(this->radioNumber);
			this->Controls->Add(this->comboBox1);
			this->Controls->Add(this->listBox1);
			this->Controls->Add(this->btnSend);
			this->Controls->Add(this->btnClose);
			this->Controls->Add(this->groupBox1);
			this->Location = System::Drawing::Point(56, 48);
			this->Name = S"Form1";
			this->Text = S"FTDI Loopback Test";
			this->Load += new System::EventHandler(this, Form1_Load);
			this->ResumeLayout(false);

		}	

	private: System::Void ButtonCancel_Click(System::Object *  sender, System::EventArgs *  e)
			{
				Application::Exit();
			}
	private: System::Void OpenPort()
			 {
				FT_STATUS ftStatus = FT_OK;
				if(dwOpenFlags == FT_LIST_NUMBER_ONLY) {
					int iSelIndex = comboBox1->SelectedIndex;
					if(iSelIndex >= 0)
						ftStatus = FT_Open(iSelIndex, &handle);
				}
				else {
					String * str;
					char cBuf[64];
					str = comboBox1->GetItemText(comboBox1->SelectedItem);
					for(int i = 0; i < str->Length; i++) {
						cBuf[i] = str->Chars[i];
					}
					ftStatus = FT_OpenEx(cBuf, dwOpenFlags, &handle);
				}
			 }

 	private: System::Void ClosePort()
			 {
				if(handle) {
					FT_Close(handle);
					handle = NULL;
				}
			 }
	private: System::Void btnSend_Click(System::Object *  sender, System::EventArgs *  e)
			 {
				FT_STATUS ftStatus = FT_OK;
				DWORD ret;
				char cBuf[100];

				// This assumes you have a loopback connector on the USB cable you are using.
				listBox1->Items->Clear();
				for(int i = 0; i < 100 ; i++) {
					cBuf[i] = (char)i;
				}

				if(handle == NULL) {
					OpenPort();
				}
				if(handle) {
					StartThread();
					ftStatus = FT_SetEventNotification(handle, FT_EVENT_RXCHAR, hEvent);
					ftStatus = FT_SetBaudRate(handle, 9600);
					ftStatus = FT_Write(handle, cBuf, 100, &ret);
				}
				else {
					MessageBox::Show("Open Failed");
					return;
				}
			 }

private: System::Void FillComboBox(UInt32 dwDescFlags)
	{
		FT_STATUS ftStatus;
		UInt32 numDevs;
		void * p1;			
		
		comboBox1->Items->Clear();

		p1 = (void*)&numDevs;
		ftStatus = FT_ListDevices(p1, NULL, FT_LIST_NUMBER_ONLY);
		
		if(ftStatus == 0) 
		{
			char cBuf[64];
			if (ftStatus == 0) 
			{
				UInt32 uDevNo;
				for(uDevNo=0; uDevNo<numDevs; uDevNo++)
				{	
					String * str;
					if(dwDescFlags == FT_LIST_NUMBER_ONLY) {
						str = Convert::ToString(uDevNo);
						comboBox1->Items->Add(str);
					}
					else {
						ftStatus = FT_ListDevices(uDevNo, cBuf, dwDescFlags);
						str = Convert::ToString(cBuf);
						comboBox1->Items->Add(str);
					}
				}
				if(comboBox1->Items->Count > 0) {
					comboBox1->SelectedIndex = 0;
				}
			}
		}
	}
private: System::Void StopThread()
		 {
			bContinue = false;
			if(hEvent)		// let the thread come out of waiting for infinite time
				SetEvent(hEvent);
			if(thrdReader && thrdReader->IsAlive) {	// stop it
			 	TimeSpan waitTime = TimeSpan(0, 0, 1);	// 1 second timeout
				if(thrdReader->Join(waitTime) != true) {
					thrdReader->Abort();	// didnt stop the thread - take more drastic action
				}
			}
		 }

private: System::Void StartThread()
		 {
			 // Create a reader thread here
			thrdReader = new Thread(new ThreadStart(this, ReadingProc));
			bContinue = true;
			thrdReader->Start();
			while (!thrdReader->IsAlive);	// wait for the thread to start
			Thread::Sleep(1000);
		 }

private: System::Void Form1_Load(System::Object *  sender, System::EventArgs *  e)
		{	
			handle = NULL;
			hEvent = CreateEvent(NULL, FALSE, FALSE, "");
			radioDescription->Checked = true;
		}

private: System::Void comboBox1_SelectedIndexChanged(System::Object *  sender, System::EventArgs *  e)
		{
			ClosePort();			 
			OpenPort();
		}

private: System::Void radioNumber_CheckedChanged(System::Object *  sender, System::EventArgs *  e)
		 {
			if(radioNumber->Checked) {
				ClosePort();			 
				dwOpenFlags = FT_LIST_NUMBER_ONLY;
				FillComboBox(dwOpenFlags);
			}
		 }

private: System::Void radioDescription_CheckedChanged(System::Object *  sender, System::EventArgs *  e)
		 {
			if(radioDescription->Checked) {
				ClosePort();			 
				dwOpenFlags = FT_OPEN_BY_DESCRIPTION;
				FillComboBox(FT_LIST_BY_INDEX | dwOpenFlags);
			}
		 }

private: System::Void radioSerial_CheckedChanged(System::Object *  sender, System::EventArgs *  e)
		 {
			if(radioSerial->Checked) {
				ClosePort();			 
				dwOpenFlags = FT_OPEN_BY_SERIAL_NUMBER;
				FillComboBox(FT_LIST_BY_INDEX | dwOpenFlags);
			}
		 }
};
}


