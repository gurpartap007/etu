#include "mainwindow.h"
#include "ui_mainwindow.h"
typedef enum
{
    MOP_IP,
    ETU_IP,
    ETU_DIRECTION,
    ETU_BOX_NO
}input_mode;
input_mode INPUT_MODE;
bool CALL_CONNECTED  = false;
bool flag            = false;
bool call_barred     = false;
char mop_ip[3];
char etu_ip[3];
char etu_box_no;
char etu_direction;
int input_count = 0;
int cursor_position = 0;
char input_characters_list[10]={'0','1','2','3','4','5','6','7','8','9'};
int call_button_fd,stop_button_fd;
int up_button_fd,enter_button_fd;
char value_call_button,value_stop_button;
char value_up_button,value_enter_button;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    call_switch_poll =  new QTimer;
    user_barred_timer = new QTimer;
    get_input_poll   =  new QTimer;
    emergency_call = new qlinphone;
    lcd = new Lcd_lib ;
   memset(mop_ip,0,sizeof(mop_ip));
    memset(etu_ip,0,sizeof(etu_ip));
    get_input_poll->setInterval(200);
    call_switch_poll->setInterval(500);
    connect(call_switch_poll,SIGNAL(timeout()),this,SLOT(check_call_button_state()));
    connect(user_barred_timer,SIGNAL(timeout()),this,SLOT(call_barred_timer_slot()));
    connect(get_input_poll,SIGNAL(timeout()),this,SLOT(check_input_buttons_state()));
    connect(this,SIGNAL(call_driver()),emergency_call,SLOT(talk_to_driver()));
    connect(emergency_call,SIGNAL(Call_ended()),this,SLOT(show_call_ended()));
    connect(emergency_call,SIGNAL(call_barred(QString)),this,SLOT(show_call_barred(QString)));
    connect(emergency_call,SIGNAL(Call_connected()),this,SLOT(show_call_connected()));
    connect(this,SIGNAL(terminate_call()),emergency_call,SLOT(end_current_call()));
    input_ip_address_for_mop();
    QString call_button_path;
    call_button_path = QString::fromStdString("/sys/class/gpio/gpio23_ph16/value");
    call_button_file.setFileName(call_button_path);
//    call_button_file.open(QFile::ReadOnly);
     QString stop_button_path;
    stop_button_path = QString::fromStdString("/sys/class/gpio/gpio22_ph15/value");
    stop_button_file.setFileName(stop_button_path);
//    stop_button_file.open(QFile::ReadOnly);
     QString up_button_path;
    up_button_path = QString::fromStdString("/sys/class/gpio/gpio23_ph16/value");
    up_button_file.setFileName(up_button_path);
  //  up_button_file.open(QFile::ReadOnly);
      QString enter_button_path;
    enter_button_path = QString::fromStdString("/sys/class/gpio/gpio22_ph15/value");
    enter_button_file.setFileName(enter_button_path);
  //  enter_button_file.open(QFile::ReadOnly);
//     get_input_poll->start();
    //up_button_fd=open("/sys/class/gpio/gpio23_ph16/value",O_RDONLY);
    //enter_button_fd=open("/sys/class/gpio/gpio22_ph15/value",O_RDONLY);
   }

MainWindow::~MainWindow()
{
    delete ui;
}


// This function will poll the state of call and stop buttons and emit appropriate signals accordingly
void MainWindow::check_call_button_state()
{
   call_button_file.open(QFile::ReadOnly);
   stop_button_file.open(QFile::ReadOnly);  
     call_button_file.seek(0);
     call_button_file.read(&value_call_button,1);
//     qDebug() << value_call_button;
     stop_button_file.seek(0);
     stop_button_file.read(&value_stop_button,1);
//     qDebug() << value_stop_button;

    if(!call_barred && !CALL_CONNECTED && (value_call_button == '0'))
    {
        qDebug() << "inside Call button routine";
        show_wait_for_driver_reply();
        emit call_driver() ;
    }
    if(CALL_CONNECTED && (value_stop_button == '0'))
    {
        qDebug() << "inside stop button routine";
        emit terminate_call() ;
    }
    call_button_file.close();
    stop_button_file.close();
//	usleep(200);
}

void MainWindow::check_input_buttons_state()
{
     up_button_file.open(QFile::ReadOnly);
     enter_button_file.open(QFile::ReadOnly);
     up_button_file.seek(0);
     up_button_file.read(&value_up_button,1);
//     qDebug() << value_up_button;
     enter_button_file.seek(0);
     enter_button_file.read(&value_enter_button,1);
//     qDebug() << value_enter_button;

    if((value_up_button == '0'))
    {
        if(input_count == 0 && cursor_position == 0)
            send_clear();
        if(input_count>=10)
            input_count = 0;
        qDebug() << "value_up_button presses" ;
        qDebug() << "count = " << input_count ;
        qDebug() << "Character = " << input_characters_list[input_count];
        lcd->send_command(0x80 + cursor_position);
        if(INPUT_MODE == ETU_DIRECTION)
        {
            qDebug() << "Inside ETU direction";
            if(!input_count)
                send_byte('L');
            else
                send_byte('R');
            input_count = !input_count;
        }
        else
        {
            send_byte(input_characters_list[input_count]);
            input_count ++;
        }
    }
    if((value_enter_button == '0') && !flag )
    {
        send_clear();
        INPUT_MODE = MOP_IP;
        flag = true;
    }
    else if(value_enter_button == '0' && flag)
    {
        if(INPUT_MODE == MOP_IP)
        {
            mop_ip[cursor_position]  = input_characters_list[input_count-1];
            input_count = 0 ;
        }
        else if(INPUT_MODE == ETU_IP)
        {
            etu_ip[cursor_position] = input_characters_list[input_count-1];
            qDebug() << "ETU IP ==" << etu_ip;
            input_count = 0 ;
        }
        else if(INPUT_MODE == ETU_DIRECTION)
        {
            if(input_count)
                etu_direction = 'L';
            else
                etu_direction = 'R';
            cursor_position ++;
            input_count = 0 ;
        }
        else if(INPUT_MODE == ETU_BOX_NO)
        {
            etu_box_no = input_characters_list[input_count-1];
            show_default_message();
            get_input_poll->stop();
            call_switch_poll->start();
            qDebug() << "MOP IP        ==" << mop_ip;
            qDebug() << "ETU IP        ==" << etu_ip;
            qDebug() << "ETU DIRECTION ==" << etu_direction;
            qDebug() << "ETU BOX NO    ==" << etu_box_no;
        }
        cursor_position ++ ;
        if(cursor_position == 2)
        {
            switch(INPUT_MODE)
            {
            case  MOP_IP:
                qDebug() << "MOP_IP CASE ";
                INPUT_MODE = ETU_IP;
                input_count = 0;
                cursor_position = 0;
                input_ip_address_for_etu();
                qDebug() << "MOP IP ==" << mop_ip;
                break;

            case  ETU_IP:
                qDebug() << "ETU_IP CASE";
                INPUT_MODE = ETU_DIRECTION ;
                input_count = 0;
                cursor_position = 0;
                set_etu_ip_address();
                input_direction_for_etu();
                qDebug() << "ETU IP ==" << etu_ip;
                break;

            case  ETU_DIRECTION:
                qDebug() << "ETU_DIRECTION CASE";
                INPUT_MODE = ETU_BOX_NO;
                input_count = 0;
                cursor_position = 0;
                input_box_number();
                qDebug() << "ETU DIRECTION ==" << etu_direction;
                break;
            }

        }
    }
 up_button_file.close();
 enter_button_file.close();
//usleep(200);
}

// show Call ended notification on lcd and after that show default message on LCD
void MainWindow::show_call_ended()
{

if(!call_barred)
{
    send_string("Emergency Call","Ended",1,5);
    sleep(2);
    CALL_CONNECTED = false;
    show_default_message();
}

}

// show Call Connected notification on lcd
void MainWindow::show_call_connected()
{
    CALL_CONNECTED = true;
    send_string("Call Connected","Please Speak Now",1,0);

}

// show default message on lcd
void MainWindow::show_default_message()
{
 send_string("Press Call","in Emergency",1,3);
}

// show message when Call is ringing remotely
void MainWindow::show_wait_for_driver_reply()
{
    send_string("Please Wait For","Driver Reply",1,4);
}

// lcd routine to write single charcter
void MainWindow::send_byte( char byte)
{
    lcd->send_character(byte);
}

// lcd routine to write string along with line 1 starting postion x and line 2 starting position y
void MainWindow::send_string(const char * line1,const char * line2,int x,int y)
{

    int counter;
    unsigned int hex_x_position,hex_y_position;
    hex_x_position = x + 0x80;
    hex_y_position = y + 0xc0;
    send_clear();
    lcd->send_command(hex_x_position);
    for(counter=0;counter  <(strlen(line1));counter++)
        lcd->send_character(line1[counter]);
    lcd->send_command(hex_y_position);
    for(counter=0;counter<(strlen(line2));counter++)
        lcd->send_character(line2[counter]);
}

// lcd routine to clear the screen
void MainWindow::send_clear()
{
    lcd->send_command(0x01);
}

void MainWindow::input_ip_address_for_mop()
{
    send_clear();
    send_string("Enter MOP","IP ADDRESS",3,5);
    qDebug() << "Inside input_ip_address()";
    get_input_poll->start();
}

void MainWindow::input_ip_address_for_etu()
{
    send_clear();
    send_string("Enter ETU","IP ADDRESS",3,5);
}

void MainWindow::input_direction_for_etu()
{
    send_clear();
    send_string("Enter Direction","Of ETU",2,5);
}

void MainWindow::input_box_number()
{
    send_clear();
    send_string("Enter ETU","Box Number",3,2);
}

void MainWindow::set_etu_ip_address()
{
    QByteArray set_manual_ip_address_command("ifconfig eth0 192.168.0.");
    set_manual_ip_address_command.append(etu_ip);
    qDebug() << "IP changed to" << set_manual_ip_address_command;
//    system(set_manual_ip_address_command.toStdString().c_str());
}

   void MainWindow::show_call_barred(QString timeout)
{
qDebug() << "Inside Call Barred Slot::" << " Timeout  " << timeout ;
send_clear();
call_barred = true;
user_barred_timer->start((timeout.toInt())*1000);
send_string("You are" ,"Barred",3,5);
}

void MainWindow::call_barred_timer_slot()
{

call_barred=false;
qDebug() << "Inside Call Barred Timer Slot::" ;
CALL_CONNECTED=false;
show_default_message();
user_barred_timer->stop();

}
