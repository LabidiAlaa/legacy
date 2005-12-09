#include <QPainter>
#include <QStringListIterator>
#include <QChar>
#include <iostream>

using namespace std;

#include "display.h"
/*
ControllPoint::ControllPoint() {
}

int ControllPoint::getLine() {
}

int ControllPoint::getElement() {
}

*/

DrawArea::DrawArea(QTextEdit *textedit, QWidget *parent)
	: QWidget(parent)
{
	width  = 2000;
	height = 2000;
	currentAngle = 45;
	text = textedit;
	drawLevel = 8;
	chainLevel = 8;
	list << "s 1 1" << "l 100 100" << "c 200 200 22 10 10 1000";
	setPalette(QPalette(QColor(255, 255, 255)));
	setZoom(1.0);
	
	connect(textedit, SIGNAL(textChanged()), this, SLOT(checkAndDraw()));
}

DrawArea::~DrawArea() {
}

void DrawArea::paintEvent(QPaintEvent * /* event */)
{
	QPainter painter(this);
	//painter.setPen(Qt::NoPen);
	painter.setBrush(Qt::black);

	QStringListIterator i(list);
	while (i.hasNext())  {
		QStringList ps = i.next().split(' ');
		switch ((ps.at(0).toAscii())[0]) {
			case 's': {
				if (ps.size() >= 3) {
					CurrentPoint.x = ps.at(1).toFloat();
					CurrentPoint.y = ps.at(2).toFloat();
				}
				break;
			} 
			case 'l': {
				if (ps.size() >= 3) {
					drawLineTo((Point) {ps.at(1).toFloat(), ps.at(2).toFloat()}, &painter);
				}
				break;
			}
			case 'c': {
				if (ps.size() >= 7) {
				drawBezier((Point) {ps.at(1).toFloat(), ps.at(2).toFloat()},
						   (Point) {ps.at(3).toFloat(), ps.at(4).toFloat()},
						   (Point) {ps.at(5).toFloat(), ps.at(6).toFloat()},
							&painter);
				}
				break;		                                  
			}
		}
	}
	getChainCode();
	cout << chain.toAscii().data() << endl;	
}

void DrawArea::setZoom(float zoom) {
	this->zoom = zoom;
	resize((int)(zoom*height), (int)(zoom*width));
}

float DrawArea::getZoom() {
	return zoom;
}


void DrawArea::checkAndDraw() {
	QString textStr = text->toPlainText();
	list = textStr.split("\n");
	repaint();
}

void DrawArea::drawLineTo(Point p, QPainter *g) {
	g->drawLine((int)(zoom*CurrentPoint.x+0.5), 
			    (int)(zoom*CurrentPoint.y+0.5),
			    (int)(zoom*p.x+0.5), 
		   	    (int)(zoom*p.y+0.5));
	CurrentPoint = p;
}

void DrawArea::drawBezier(Point p2, Point p3, Point p4, QPainter *g) {
	drawBezierRec(CurrentPoint, p2, p3, p4, drawLevel, g);
	CurrentPoint = p4;
}

void DrawArea::drawBezierRec(Point p1, Point p2, Point p3, Point p4, int level, QPainter *g) {
	if (level == 0) {
		drawLineTo(p4, g);
	} else {
		Point l1 = p1;
		Point l2 = midpoint(p1, p2);
		Point h  = midpoint(p2, p3);
		Point r3 = midpoint(p3, p4);
		Point r4 = p4;
		Point l3 = midpoint(l2, h);
		Point r2 = midpoint(r3, h);
		Point l4 = midpoint(l3, r2);
		Point r1 = l4;
		drawBezierRec(l1, l2, l3, l4, level-1, g);
		drawBezierRec(r1, r2, r3, r4, level-1, g);
	}
}


Point DrawArea::midpoint(Point p1, Point p2) {
	return (Point) {(p1.x+p2.x)/2.0, (p1.y+p2.y)/2.0};
} 	

void DrawArea::getChainCode() {
	QStringListIterator i(list);
	
	while (i.hasNext())  {
		QStringList ps = i.next().split(' ');
		switch ((ps.at(0).toAscii())[0]) {
			case 's': {
				if (ps.size() >= 3) {
					CurrentPoint.x = ps.at(1).toFloat();
					CurrentPoint.y = ps.at(2).toFloat();
					startChain((int)(CurrentPoint.x), (int)(CurrentPoint.y));
				}
				break;
			} 
			case 'l': {
				if (ps.size() >= 3) {
					chainLineTo((Point) {ps.at(1).toFloat(), ps.at(2).toFloat()});
				}
				break;
			}
			case 'c': {
				if (ps.size() >= 7) {
				chainBezier((Point) {ps.at(1).toFloat(), ps.at(2).toFloat()},
							(Point) {ps.at(3).toFloat(), ps.at(4).toFloat()},
							(Point) {ps.at(5).toFloat(), ps.at(6).toFloat()});
				}
				break;		                                  
			}
		}
	}	
}

void DrawArea::chainLineTo(Point p) {
	int i, dx, dy, sdx, sdy, dxabs, dyabs, x, y, px, py;
	dx = (int)(p.x+0.5) - (int)(CurrentPoint.x+0.5);
	dy = (int)(p.y+0.5) - (int)(CurrentPoint.y+0.5);
	dxabs = dx >= 0 ? dx: -dx; //abs
	dyabs = dy >= 0 ? dy: -dy; //abs
	sdx = dx >= 0 ? 1: -1;     //sign
	sdy = dy >= 0 ? 1: -1;     //sign
	x = dyabs >> 1;
	y = dxabs >> 1;
	px = (int)(CurrentPoint.x+0.5);
	py = (int)(CurrentPoint.y+0.5);
	addToChain(px, py);
	if (dxabs >= dyabs) { 	  // the line is more horizontal than vertical
		for (i = 0; i < dxabs; i++) {
			y += dyabs;
			if (y >= dxabs) {
				y -= dxabs;
				py += sdy;
			}
			px += sdx;
			addToChain(px, py);
		}
	} else { 				 // the line is more vertical than horizontal
		for (i = 0; i < dyabs; i++) {
			x += dxabs;
			if (x >= dyabs) {
				x -= dyabs;
				px += sdx;
			}
			py += sdy;
			addToChain(px, py);
		}
	}
	CurrentPoint = p;
}

void DrawArea::chainBezier(Point p2, Point p3, Point p4) {
	chainBezierRec(CurrentPoint, p2, p3, p4, chainLevel);
	CurrentPoint = p4;
}

void DrawArea::chainBezierRec(Point p1, Point p2, Point p3, Point p4, int level) {
	if (level == 0) {
		chainLineTo(p4);
	} else {
		Point l1 = p1;
		Point l2 = midpoint(p1, p2);
		Point h  = midpoint(p2, p3);
		Point r3 = midpoint(p3, p4);
		Point r4 = p4;
		Point l3 = midpoint(l2, h);
		Point r2 = midpoint(r3, h);
		Point l4 = midpoint(l3, r2);
		Point r1 = l4;
		chainBezierRec(l1, l2, l3, l4, level-1);
		chainBezierRec(r1, r2, r3, r4, level-1);
	}
}

void DrawArea::startChain(int px, int py) {
	chain = "";
	QString help;
	chain.append(help.setNum(px));
	chain.append(';');
	chain.append(help.setNum(py));
	
	chainPosX = px;
	chainPosY = py;
}

void DrawArea::addToChain(int px, int py) {
	int dx = px - chainPosX;
	int dy = py - chainPosY;
	QChar addChain = ' ';
	bool skip = false;	
	if (dy == -1) {
		switch (dx) {
			case -1: addChain = 'H'; break;
			case  0: addChain = 'A'; break;
			case  1: addChain = 'B'; break;
			default: addChain = 'X'; break;
		}
	} else if (dy == 0) {
		switch (dx) {
			case -1: addChain = 'G'; break;
			case  0: skip     = true; break;
			case  1: addChain = 'C'; break;
			default: addChain = 'X'; break;
		}
	} else if (dy == 1) {
		switch (dx) {
			case -1: addChain = 'F'; break;
			case  0: addChain = 'E'; break;
			case  1: addChain = 'D'; break;
			default: addChain = 'X'; break;
		}
	} else {
		addChain = 'X';
	}

	if (!skip) {
		chain.append(addChain);
	}
	chainPosX = px;
	chainPosY = py;
}
