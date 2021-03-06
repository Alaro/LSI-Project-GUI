#include "LSIProjectGUI.h"

LSIProjectGUI::LSIProjectGUI(QWidget *parent)
	: QMainWindow(parent)
{

	ui.setupUi(this);

	//Timer which updates the gui and takes images
	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(update()));
	//Connects the serial port which is used to control the laser
	port = new QSerialPort(this);
	port->setPortName("COM3");
	port->open(QIODevice::WriteOnly);
	port->setRequestToSend(false);

	
	camera.Connect(0);
	camera.StartCapture();
	camera.SetVideoModeAndFrameRate(VIDEOMODE_1280x960Y8, FRAMERATE_60); //Changes the resolution of the camera

	refresh_rate = 200;
	exposure_time = 20;
	lasca_area = 5;
	//For webcam
	VideoCapture temp(0);
	webcam = temp;
	should_i_run = true;

	//LSIProjectGUI::makePlot();
	graph_update=0;
	x_min = -1;
	x_max = 5;
	ambient_ligth_refresh_rate = 5;
	ambient_ligth_refresh_rate_count = 0;
	//port = new QSerialPort(this);

	
	// give the graph axes some labels:
	ui.customPlot->xAxis->setLabel("Time");
	ui.customPlot->yAxis->setLabel("Mean Contrast");
	ui.customPlot->xAxis->setRange(x_min, x_max);
	ui.customPlot->yAxis->setRange(0, 50);

	////Declare a Property struct.
	//Property prop;
	////Define the property to adjust.
	//prop.type = GAIN;
	////Ensure auto-adjust mode is off.
	//prop.autoManualMode = false;
	////Ensure the property is set up to use absolute value control.
	//prop.absControl = true;
	////Set the absolute value of gain to 10.5 dB.
	//prop.absValue = 10.5;
	////Set the property.
	//camera.SetProperty(&prop);
	load_init();
}

void LSIProjectGUI::set_exposure(int time)
{
	//Declare a Property struct.
	Property prop;
	//Define the property to adjust.
	prop.type = SHUTTER;
	//Ensure the property is on.
	prop.onOff = true;
	//Ensure auto-adjust mode is off.
	prop.autoManualMode = false;
	//Ensure the property is set up to use absolute value control.
	prop.absControl = true;
	//Set the absolute value of shutter to X ms.
	prop.absValue = time;
	//Set the property.
	camera.SetProperty(&prop);
}

void LSIProjectGUI::take_laser_image()
{
	laser_ON();
	camera.Connect(0);
	camera.StartCapture();
	camera.RetrieveBuffer(&rawImage);
	rawImage.Convert(FlyCapture2::PIXEL_FORMAT_BGR, &rgbImage);
	unsigned int rowBytes = (double)rgbImage.GetReceivedDataSize() / (double)rgbImage.GetRows(); //Converts the Image to Mat
	Main_Image_CV = cv::Mat(rgbImage.GetRows(), rgbImage.GetCols(), CV_8UC3, rgbImage.GetData(), rowBytes);
	/*webcam >> Main_Image_CV;
	webcam >> Main_Image_CV;*/
	//remove_ambient_ligth_and_black_image();
	laser_OF();
}

void LSIProjectGUI::take_ambient_light_image()
{
	camera.Connect(0);
	camera.StartCapture();
	camera.RetrieveBuffer(&rawImage);
	rawImage.Convert(FlyCapture2::PIXEL_FORMAT_BGR, &rgbImage);
	unsigned int rowBytes = (double)rgbImage.GetReceivedDataSize() / (double)rgbImage.GetRows(); //Converts the Image to Mat
	Main_Image_CV_for_ambient_light = cv::Mat(rgbImage.GetRows(), rgbImage.GetCols(), CV_8UC3, rgbImage.GetData(), rowBytes);
	/*webcam >> Main_Image_CV;
	webcam >> Main_Image_CV;*/

	//remove_ambient_ligth_and_black_image();
	if (!Black_im.empty()) // Removes the black image when taken.
	{
		absdiff(Main_Image_CV_for_ambient_light, Black_im, Main_Image_CV_for_ambient_light);
	}
	Raw_im = Main_Image_CV_for_ambient_light;
}

void LSIProjectGUI::remove_ambient_ligth_and_black_image()
{
	if (!Black_im.empty()) // Removes the black image when taken.
	{
		absdiff(Main_Image_CV, Black_im, Main_Image_CV);
	}
	if (!Raw_im.empty()) // Removes the ambient light when image taken.
	{
		absdiff(Main_Image_CV, Raw_im, Main_Image_CV);
	}
}

void LSIProjectGUI::uppdate_ambientlight()
{
	if (!static_ambient_ligth) 
	{
		if (ambient_ligth_refresh_rate_count == 0)
		{
			take_ambient_light_image();
			ambient_ligth_refresh_rate_count = ambient_ligth_refresh_rate;
		}
		else
		{
			ambient_ligth_refresh_rate_count--;
		}
	}
}

void LSIProjectGUI::do_contrast()
{
	Main_Image_CV = CalculateContrast2(Main_Image_CV, lasca_area, Calib_Still, Calib_Moving); //QImage::Format_RGB888 QImage::Format_Grayscale8
	Add_Contrast_Image(Main_Image_CV);
	TemporalFiltering(Contrast_Images);
	cv::resize(Main_Image_CV, Main_Image_CV, cv::Size(640, 480), 0, 0, cv::INTER_CUBIC);
	Main_Image_CV = one_divided_by_kontrast_squared(Main_Image_CV);
}

void LSIProjectGUI::load_init()
{
	ifstream read;
	read.open("settings//settings.txt");

	if (read.is_open())
	{
		read >> refresh_rate; //Reads the variables in the order they come in the settings.txt
	}
	else
	{
		//Set standard values instead and write and error.
		refresh_rate = 5;
	}
	Black_im = imread("images//morkerBild.png");

}

void LSIProjectGUI::save_init()
{
	ofstream write;
	write.open("settings//settings.txt", std::ofstream::trunc);
	write << refresh_rate; //Add any other variables to be saved.
}



void LSIProjectGUI::update()
{
	if (should_i_run) {
		uppdate_ambientlight();
		take_laser_image();
		remove_ambient_ligth_and_black_image();
		do_contrast();


		Main_Image = QPixmap::fromImage(QImage((unsigned char*)Main_Image_CV.data, Main_Image_CV.cols, Main_Image_CV.rows, QImage::Format_RGB888)); //Converts Mat to QPixmap
		ui.videoLabel->setPixmap(Main_Image);

		// vector for ROI colours
		QVector<QColor> ROI_Colors{QColor("red"), QColor("darkBlue"), QColor("Yellow"), QColor("cyan"), QColor("darkMagenta"), QColor("green"), QColor("darkRed"), QColor("blue"), QColor("darkYellow"), QColor("darkCyan"), QColor("magenta"), QColor("darkGreen") };
		
		for (int f = 0; f < List_Of_ROI.size(); f++)
		{
			QPainter painter(&Main_Image);
			color_index = List_Of_ROI.at(f).ROI_Colour - 1;
			pen.setBrush(ROI_Colors.at(color_index)); // sets new color for each ROI
			painter.setPen(pen); //sets pen settings from above to painter
			int x = List_Of_ROI.at(f).Get_ROI_Location().at(0);
			int y = List_Of_ROI.at(f).Get_ROI_Location().at(1);
			int ROI_w = List_Of_ROI.at(f).Get_ROI_Region().at(0);
			int ROI_h = List_Of_ROI.at(f).Get_ROI_Region().at(1);
			painter.drawRect(x, y, ROI_w, ROI_h);
			ui.videoLabel->setPixmap(Main_Image);
		}
	}
	//Fel att ta in frame objekt nu...


	if (Is_ROI_Button_Is_Pressed)
	{
		QPainter painter(&Main_Image);
		painter.setPen(pen); //sets pen settings from above to painter
		painter.drawRect(x_Start_ROI_Coordinate, y_Start_ROI_Coordinate, ROI_Width, ROI_Height);
		ui.videoLabel->setPixmap(Main_Image);
	}


	// plots graphs
	if (!List_Of_ROI.empty()) // prevents program from crashing if vector is empty
	{
		graph_update++;
		// calculates average for all ROIs and saves them in a vector
		// gets overwritten until graph_update == 5
		QVector<double> ROI_Averages = Calc_ROI_Average(Main_Image_CV, List_Of_ROI); // Main_Image_CV not right perfusion image yet
		QVector<qreal> ROI_Averages_qreal;

		for (int i = 0; i < ROI_Averages.size(); i++)
		{
			// saves ROI Averages before they get overwritten
			QVector<qreal> firstvector;
			firstvector.append(ROI_Averages.at(0));
			Multiple_ROI_Averages.append(firstvector);

			ROI_Averages_qreal.append(ROI_Averages.at(i)); // makes QVector out of vector
			Multiple_ROI_Averages[i].append(ROI_Averages_qreal.at(i));
		}


		if (graph_update == 5) // after 5*200ms = 1s graphs update
		{
			//for (int k = 0; k < ROI_Averages.size(); k++) // loops through ROI vector
			for (int k = 0; k < List_Of_ROI.size(); k++)
			{
				QVector<qreal> x(Multiple_ROI_Averages[k].count());
				for (int i = 0; i < Multiple_ROI_Averages[k].count(); ++i)
				{
					x[i] = i;
				}

				QVector<QColor> ROI_Colors{ QColor("red"), QColor("darkBlue"), QColor("Yellow"), QColor("cyan"), QColor("darkMagenta"), QColor("green"), QColor("darkRed"), QColor("blue"), QColor("darkYellow"), QColor("darkCyan"), QColor("magenta"), QColor("darkGreen") };
				int color = List_Of_ROI.at(k).ROI_Colour - 1;

				ui.customPlot->addGraph();
				ui.customPlot->graph(k)->setData(x, Multiple_ROI_Averages[k]);
				ui.customPlot->graph(k)->setPen(QPen(ROI_Colors.at(color)));
				ui.customPlot->replot();
				ui.customPlot->xAxis->setRange(x_min, x_max);

				if (k == 0 & Multiple_ROI_Averages[k].count() >= 6)
				{
					x_min++;
					x_max++;
				}
			}
			graph_update = 0;
		}
	}
}


void LSIProjectGUI::on_startButton_clicked() {
	timer->start(refresh_rate);
}

void LSIProjectGUI::on_stopButton_clicked() {
	ui.button_test->setText("STOP!");
	timer->stop();
	port->setRequestToSend(false);
}

void LSIProjectGUI::on_createROIButton_clicked()
{
	Is_ROI_Button_Is_Pressed = true; // to start mouseEvent functions
}

void LSIProjectGUI::on_removeROIButton_clicked()
{
	if (!List_Of_ROI.empty()) // prevents program from crashing if vector is empty
	{		
		int selectedROI = ui.listROI->currentRow();
		ui.button_test->setText(QString::number(selectedROI));

		List_Of_ROI.erase(List_Of_ROI.begin() + selectedROI); 
		delete ui.listROI->takeItem(selectedROI); 

		// removes graph
		Multiple_ROI_Averages.erase(Multiple_ROI_Averages.begin() + selectedROI);
	}
}


void LSIProjectGUI::mousePressEvent(QMouseEvent *event)
{
	if (Is_ROI_Button_Is_Pressed)
	{
		ROI_Width = 0; //so if a new ROI is drawn, the old one doesn't appear right next to it while drawing the new one
		ROI_Height = 0;

		Start_Click_Coordinates = event->pos(); // in whole GUI, not in videoLabel
		x_Start_Click_Coordinate = Start_Click_Coordinates.x();
		y_Start_Click_Coordinate = Start_Click_Coordinates.y();

		videoLabel_Coordinates = ui.videoLabel->pos();
		x_videoLabel_Coordinate = videoLabel_Coordinates.x();
		y_videoLabel_Coordinate = videoLabel_Coordinates.y();

		x_Start_ROI_Coordinate = x_Start_Click_Coordinate - x_videoLabel_Coordinate;
		y_Start_ROI_Coordinate = y_Start_Click_Coordinate - y_videoLabel_Coordinate;
		Start_ROI_Coordinates = QPoint(x_Start_ROI_Coordinate, y_Start_ROI_Coordinate);

		QString x_videoLabel_string = QString::number(x_videoLabel_Coordinate);
		QString y_videoLabel_string = QString::number(y_videoLabel_Coordinate);
		ui.button_test->setText(x_videoLabel_string + "<x  y>" + y_videoLabel_string); // just to see videoLabel coordinates

		QString x_Start_Click_Coordinates_string = QString::number(x_Start_Click_Coordinate);
		QString y_Start_Click_Coordinates_string = QString::number(y_Start_Click_Coordinate);
		ui.button_test->setText(x_Start_Click_Coordinates_string + "<x  y>" + y_Start_Click_Coordinates_string); // just to see GUI window coordinates

		QString x_Start_ROI_Coordinate_string = QString::number(x_Start_ROI_Coordinate);
		QString y_Start_ROI_Coordinate_string = QString::number(y_Start_ROI_Coordinate);
		ui.button_test->setText(x_Start_ROI_Coordinate_string + "<x  y>" + y_Start_ROI_Coordinate_string); // just to see ROI coordinates
	}
}


void LSIProjectGUI::mouseMoveEvent(QMouseEvent *event)
{
	if (Is_ROI_Button_Is_Pressed)
	{
		temp_Main_Image = Main_Image;

		// same color vector as in update function
		QVector<QColor> ROI_Colors{QColor("red"), QColor("darkBlue"), QColor("Yellow"), QColor("cyan"), QColor("darkMagenta"), QColor("green"), QColor("darkRed"), QColor("blue"), QColor("darkYellow"), QColor("darkCyan"), QColor("magenta"), QColor("darkGreen") };

		// needs this manually for the first rectangle, otherwise it cannot be seen while it's drawn
		if (List_Of_ROI.size() == 0)
		{
			QPainter painter(&temp_Main_Image);
			pen;
			pen.setWidth(4);
			pen.setBrush(Qt::red);
			painter.setPen(pen);
			painter.drawRect(QRect(Start_ROI_Coordinates, event->pos() - videoLabel_Coordinates));
			ui.videoLabel->setPixmap(temp_Main_Image);
		}

		// then loop for all other rectangles
		for (int f = 1; f < List_Of_ROI.size() + 1; f++)
		{
			QPainter painter(&temp_Main_Image);
			pen;  // creates a default pen
			pen.setWidth(4);
			color_index = List_Of_ROI.at(f-1).ROI_Colour;
			pen.setBrush(ROI_Colors.at(color_index)); // sets new color for each ROI
			painter.setPen(pen); //sets pen settings to painter

			painter.drawRect(QRect(Start_ROI_Coordinates, event->pos() - videoLabel_Coordinates));
			ui.videoLabel->setPixmap(temp_Main_Image);
		}
	}
}


void LSIProjectGUI::mouseReleaseEvent(QMouseEvent *event)
{
	if (Is_ROI_Button_Is_Pressed)
	{
		QPoint End_Click_Coordinates = event->pos();
		int x_End_Click_Coordinate = End_Click_Coordinates.x();
		int y_End_Click_Coordinate = End_Click_Coordinates.y();

		x_End_ROI_Coordinate = x_End_Click_Coordinate - x_videoLabel_Coordinate;
		y_End_ROI_Coordinate = y_End_Click_Coordinate - y_videoLabel_Coordinate;

		ROI_Width = x_End_ROI_Coordinate - x_Start_ROI_Coordinate;
		ROI_Height = y_End_ROI_Coordinate - y_Start_ROI_Coordinate;

		QString Width_string = QString::number(ROI_Width);
		QString Height_string = QString::number(ROI_Height);
		ui.button_test->setText(Width_string + "<Width   Hight>" + Height_string); // just to check width and height of ROI

		// L�gger in Nya ROI i listan i GUIt
		i++;
		ui.listROI->addItem("ROI" + QString::number(i));

		// skapar vektorer f�r att skapa nytt ROI object
		vector<int> ROIlocation;
		vector<int> ROIregion;
		int ROIcolor;

		// same color vector as in update function
		QVector<QColor> ROI_Colors{ QColor("red"), QColor("darkBlue"), QColor("Yellow"), QColor("cyan"), QColor("darkMagenta"), QColor("green"), QColor("darkRed"), QColor("blue"), QColor("darkYellow"), QColor("darkCyan"), QColor("magenta"), QColor("darkGreen") };
		
		// should give 1 to ROI_Colors.size() so we can loop through ROI_Colors vector but doesn't really work; crashes after last color
		ROIcolor = i - ROI_Colors.size() * floor((i - 1) / ROI_Colors.size()); // floor = round down
		//
		ROIregion.push_back(abs(ROI_Width));
		ROIregion.push_back(abs(ROI_Height));
		//
		if (ROI_Height<0 && ROI_Width<0) {
			ROIlocation.push_back(x_End_ROI_Coordinate);
			ROIlocation.push_back(y_End_ROI_Coordinate);
		}
		else if (ROI_Height>0 && ROI_Width>0) {
			ROIlocation.push_back(x_Start_ROI_Coordinate);
			ROIlocation.push_back(y_Start_ROI_Coordinate);
		}
		else if (ROI_Height>0 && ROI_Width<0) {
			ROIlocation.push_back(x_End_ROI_Coordinate);
			ROIlocation.push_back(y_Start_ROI_Coordinate);
		}
		else if (ROI_Height<0 && ROI_Width>0) {
			ROIlocation.push_back(x_Start_ROI_Coordinate);
			ROIlocation.push_back(y_End_ROI_Coordinate);
		}

		ROI ROI(ROIlocation, ROIregion, ROIcolor);
		List_Of_ROI.push_back(ROI);
		// selects first row by default (program crashes if we remove ROI when nothing is selected)
		ui.listROI->setCurrentRow(0);
	}
	Is_ROI_Button_Is_Pressed = false; // only make one ROI at a time
}

// Tittar om bilden �r delbar med vald LASCA area
void LSIProjectGUI::on_LASCAarea_valueChanged() {
	//T�mmer error labeln
	should_i_run = false;
	ui.error_LASCA_label->setText("");
	int LASCA = ui.LASCAarea->value();
	if (LASCA > 0) {
		// Tar ut storleken p� bilden
		QSize im_size = Main_Image.size();
		int h = im_size.height();
		int w = im_size.width();
		// S�tter ett error om n�gon av h�jd/bredd inte �r delbar med LASCA arean
		if (h % LASCA != 0 && w % LASCA != 0) {
			ui.error_LASCA_label->setText("Change to a value that the image is divadible by!");
		}
		else { lasca_area = LASCA; should_i_run = true; }
	}
	else
	{
		ui.error_LASCA_label->setText("Choose a non-zero value!");
	}
}


void LSIProjectGUI::on_exposuretime_valueChanged()
{
	int t = ui.exposuretime->value();
	//ui.error_LASCA_label->setText(Width_string);
	set_exposure(t);
}

void LSIProjectGUI::makePlot(QVector<qreal> a) // we don't use this function any more at the moment
{
	// generate some data:
	QVector<qreal> x(a.count()); 
	for (int i = 0; i<a.count(); ++i)
	{
		x[i] = i; 
	}
	ui.customPlot->addGraph();
	ui.customPlot->graph(0)->setData(x, a);
	ui.customPlot->replot();
	ui.customPlot->xAxis->setRange(x_min, x_max);

	if (a.count() >= 6 ) {
		x_min++;
		x_max++;
	}
		
	//ui.customPlot->xAxis->setRange(x_min, x_max);

}

void LSIProjectGUI::Add_Contrast_Image(Mat New_Cont_Image)
{
	Contrast_Images.push_back(New_Cont_Image);

	if (Contrast_Images.size() > 5) // Number of images to do temporal filtering over.
	{
		Contrast_Images.erase(Contrast_Images.begin());
	}
}

// Function used to generate the image to remove ambient light and unevenness in the camera.
Mat LSIProjectGUI::Help_Average_Images_RT(int Num_Images)
{
	//should_i_run = false;
	timer->stop();
	camera.Connect();
	Mat Ave_Image;

	for (int i = 0; i < Num_Images; i++) {
		camera.RetrieveBuffer(&rawImage);

		rawImage.Convert(FlyCapture2::PIXEL_FORMAT_BGR, &rgbImage);
		unsigned int rowBytes = (double)rgbImage.GetReceivedDataSize() / (double)rgbImage.GetRows(); //Converts the Image to Mat
		Ave_Image = cv::Mat(rgbImage.GetRows(), rgbImage.GetCols(), CV_8UC3, rgbImage.GetData(), rowBytes) / Num_Images + Ave_Image;
	}
	//should_i_run = true;
	return(Ave_Image);
}

void LSIProjectGUI::on_AmbL_Button_clicked()
{
	static_ambient_ligth = true; 
	Raw_im = Help_Average_Images_RT(100);
	imwrite("images//ambientBild.png", Raw_im);
	ui.button_test->setText("Amb klart!");
}

void LSIProjectGUI::on_Dark_Button_clicked()
{
	Black_im = Help_Average_Images_RT(100);
	imwrite("images//morkerBild.png", Black_im);
	ui.button_test->setText("Klar mork!");
}

void LSIProjectGUI::on_laserButton_clicked()
{
	if (!should_i_run) //If since it may screw something up if allowed to swithc while running
	{
		port->setRequestToSend(laser_switch);
		laser_switch = !laser_switch;
	}
}

void LSIProjectGUI::laser_OF()
{
	laser_switch = true;
	port->setRequestToSend(laser_switch);
}
void LSIProjectGUI::laser_ON()
{
	laser_switch = false;
	port->setRequestToSend(laser_switch);
}


void LSIProjectGUI::on_patientName_textEdited(const QString &text)
{
	string time  = QTime::currentTime().toString().toStdString();
	filename = text.toStdString();
	
	Video_Contrast.open("images\\" + filename + "_contrast.avi", CV_FOURCC('M', 'J', 'P', 'G'), 10, cv::Size(1288, 964), true);
	Video_Base.open("images\\" + filename + "_base.avi", CV_FOURCC('M', 'J', 'P', 'G'), 10, cv::Size(1288, 964), true);

}


void LSIProjectGUI::on_CalibrateStill_Button_clicked()
{
	Mat Calib_Image_Still = Help_Average_Images_RT(10);
	if (!Black_im.empty()) // Removes the black image when taken.
	{
		absdiff(Calib_Image_Still, Black_im, Calib_Image_Still);
	}
	if (!Raw_im.empty()) // Removes the ambient light when image taken.
	{
		absdiff(Calib_Image_Still, Raw_im, Calib_Image_Still);
	}

	Calib_Image_Still = CalculateContrast2(Calib_Image_Still, lasca_area, 0, 0);
	Calib_Still = mean(Calib_Image_Still).val[0];
}

void LSIProjectGUI::on_CalibrateMoving_Button_clicked()
{
	Mat Calib_Image_Moving;
	camera.RetrieveBuffer(&rawImage);

	rawImage.Convert(FlyCapture2::PIXEL_FORMAT_BGR, &rgbImage);
	unsigned int rowBytes = (double)rgbImage.GetReceivedDataSize() / (double)rgbImage.GetRows(); //Converts the Image to Mat
	Calib_Image_Moving = Mat(rgbImage.GetRows(), rgbImage.GetCols(), CV_8UC3, rgbImage.GetData(), rowBytes);

	if (!Black_im.empty()) // Removes the black image if taken.
	{
		absdiff(Calib_Image_Moving, Black_im, Calib_Image_Moving);
	}
	if (!Raw_im.empty()) // Removes ambient light if image taken.
	{
		absdiff(Calib_Image_Moving, Raw_im, Calib_Image_Moving);
	}

	Calib_Image_Moving = CalculateContrast2(Calib_Image_Moving, lasca_area, 0, 0);
	Calib_Moving = mean(Calib_Image_Moving).val[0];
}