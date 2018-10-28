#include "MagickMeter.h"

double GetLength(double dx, double dy) noexcept;

struct Shape
{
    Magick::Drawable shape;
    Magick::DrawableList attributes;
    double canvasWidth;
    double canvasHeight;
};

Magick::Coordinate GetProportionPoint(
    Magick::Coordinate point,
    double segment,
	double length,
    double dx,
    double dy);

void DrawRoundedCorner(
    Magick::VPathList &drawList,
    const Magick::Coordinate &angularPoint,
	const Magick::Coordinate &p1,
    const Magick::Coordinate &p2,
    double radius,
    bool start);

Shape* CreateEllipse(WSVector &inParaList, ImgContainer &outImg);
Shape* CreateRectangle(WSVector &inParaList, ImgContainer &outImg);
Shape* CreateRectangleCustomCorner(WSVector &inParaList, std::vector<double> &inCornerList, ImgContainer &outImg);
Shape* CreatePolygon(WSVector &inParaList, std::vector<double> &inCornerList, ImgContainer &outImg);

class PathCubicSmoothCurvetoAbs : public Magick::VPathBase
{
public:
	// Draw a single curve
	PathCubicSmoothCurvetoAbs(const Magick::PathQuadraticCurvetoArgs &args_)
		: _args(1, args_) {}
	// Draw multiple curves
	PathCubicSmoothCurvetoAbs(const Magick::PathQuadraticCurvetoArgsList &args_)
		: _args(args_) {}

	// Copy constructor
	PathCubicSmoothCurvetoAbs(const PathCubicSmoothCurvetoAbs& original_)
		: VPathBase(original_), _args(original_._args) {}

	/*virtual*/ ~PathCubicSmoothCurvetoAbs(void) {};

	// Operator to invoke equivalent draw API call
	/*virtual*/ void operator() (MagickCore::DrawingWand *context_) const override
	{
		for (Magick::PathQuadraticCurvetoArgsList::const_iterator p = _args.begin();
			p != _args.end(); p++)
		{
			DrawPathCurveToSmoothAbsolute(context_, p->x1(), p->y1(), p->x(), p->y());
		}
	}

	// Return polymorphic copy of object
	/*virtual*/
	VPathBase* copy() const override
	{
        return new PathCubicSmoothCurvetoAbs(*this);
	}

private:
	Magick::PathQuadraticCurvetoArgsList _args;
};

BOOL Measure::CreateShape(ImgType shapeType, LPCWSTR shapeParas, WSVector &config, ImgContainer &out)
{
	Shape* outShape = nullptr;

	double strokeWidth = 0;
    int strokeAlign = 0;
    auto strokeColor = Magick::DrawableStrokeColor(Magick::Color("black"));

	Magick::Geometry customCanvas;
	switch (shapeType)
	{
	case ELLIPSE:
	{
		auto paraList = Utils::SeparateParameter(shapeParas, 0);
		const size_t pSize = paraList.size();
		if (pSize < 3)
		{
			RmLog(rm, LOG_ERROR, L"Ellipse: Not enough parameter.");
			return FALSE;
		}

        outShape = CreateEllipse(paraList, out);

		break;
	}
	case RECTANGLE:
	{
        auto customParaList = Utils::SeparateList(shapeParas, L";", 2, L"");
        auto paraList = Utils::SeparateParameter(customParaList.at(0), 0);
        auto cornerList = Utils::SeparateParameter(customParaList.at(1), 0);

        if (paraList.size() < 4)
        {
            RmLog(rm, LOG_ERROR, L"Rectangle: Not enough parameter.");
            return FALSE;
        }

        const size_t cSize = cornerList.size();
        if (cSize > 0)
        {
            std::vector<double> parsedCorner;
            switch (cSize)
            {
            case 1:
            {
                const double corner = MathParser::ParseDouble(cornerList.at(0));

                for (int i = 0; i < 4; i++)
                    parsedCorner.push_back(corner);
                break;
            }
            case 2:
            {
                const double corner1 = MathParser::ParseDouble(cornerList.at(0));
                const double corner2 = MathParser::ParseDouble(cornerList.at(1));

                parsedCorner.push_back(corner1);
                parsedCorner.push_back(corner2);
                parsedCorner.push_back(corner1);
                parsedCorner.push_back(corner2);
                break;
            }
            case 3:
            {
                const double corner2 = MathParser::ParseDouble(cornerList.at(1));

                parsedCorner.push_back(MathParser::ParseDouble(cornerList.at(0)));
                parsedCorner.push_back(corner2);
                parsedCorner.push_back(MathParser::ParseDouble(cornerList.at(2)));
                parsedCorner.push_back(corner2);
                break;
            }
            case 4:
            {
                parsedCorner.push_back(MathParser::ParseDouble(cornerList.at(0)));
                parsedCorner.push_back(MathParser::ParseDouble(cornerList.at(1)));
                parsedCorner.push_back(MathParser::ParseDouble(cornerList.at(2)));
                parsedCorner.push_back(MathParser::ParseDouble(cornerList.at(3)));
                break;
            }
            }

            outShape = CreateRectangleCustomCorner(paraList, parsedCorner, out);
        }
        else
        {
            outShape = CreateRectangle(paraList, out);
        }

		break;
	}
	case POLYGON:
	{
        auto customParaList = Utils::SeparateList(shapeParas, L";", 2, L"");
        auto paraList = Utils::SeparateParameter(customParaList.at(0), 0);
        auto cornerList = Utils::SeparateParameter(customParaList.at(1), 0);

		const size_t pSize = paraList.size();
		if (pSize < 4)
		{
			RmLogF(rm, LOG_ERROR, L"Polygon %s: Not enough parameter", shapeParas);
			return FALSE;
		}
		const double side = MathParser::ParseDouble(paraList.at(2));
		if (side < 3)
		{
			RmLogF(rm, LOG_ERROR, L"Polygon %s: Invalid number of sides.", shapeParas);
			return FALSE;
		}

        double roundCorner = 0;
        if (pSize > 6) roundCorner = abs(MathParser::ParseDouble(paraList.at(6)));

        const size_t cSize = cornerList.size();
        std::vector<double> parsedCorner;
        if (cSize > 0)
        {
            if (cSize >= side)
            {
                for (int i = 0; i < side; i++)
                {
                    parsedCorner.push_back(
                        MathParser::ParseDouble(cornerList.at(i))
                    );
                }
            }
            else
            {
                RmLogF(rm, LOG_WARNING, L"Polygon: %d custom round corner parameters are not enough for %d sides polygon.", cornerList.size(), static_cast<int>(side));
            }
        }

        if (parsedCorner.size() == 0 && roundCorner > 0)
        {
            for (int i = 0; i < side; i++)
                parsedCorner.push_back(roundCorner);
        }

        outShape = CreatePolygon(paraList, parsedCorner, out);
		break;
	}
	case PATH:
	{
		LPCWSTR pathString = RmReadString(rm, shapeParas, L"");
		if (wcslen(pathString) == 0)
		{
            RmLogF(rm, LOG_ERROR, L"Path %s: Not found", shapeParas);
            break;
		}

        WSVector pathList = Utils::SeparateList(pathString, L"|", 0);

        Magick::VPathList drawList;
        double curX = 0.0;
        double curY = 0.0;

        WSVector startPoint = Utils::SeparateParameter(pathList.at(0), 0);
        if (startPoint.size() >= 2)
        {
            curX = MathParser::ParseDouble(startPoint.at(0));
            curY = MathParser::ParseDouble(startPoint.at(1));
            drawList.push_back(Magick::PathMovetoAbs(Magick::Coordinate(
                curX,
                curY
            )));
        }
        else
        {
            RmLogF(rm, LOG_ERROR, L"%s is invalid start point.", pathList.at(0).c_str());
            return FALSE;
        }

        double imgX1 = curX;
        double imgY1 = curY;
        double imgX2 = curX;
        double imgY2 = curY;

        for (auto &path : pathList)
        {
            std::wstring tempName, tempPara;
            Utils::GetNamePara(path, tempName, tempPara);
            ParseInternalVariable(tempPara, out);

            LPCWSTR name = tempName.c_str();

            WSVector segmentList = Utils::SeparateParameter(tempPara, 0);
            const size_t segmentSize = segmentList.size();

            if (_wcsicmp(name, L"LINETO") == 0)
            {
                if (segmentSize < 2)
                {
                    RmLogF(rm, LOG_WARNING, L"%s is invalid LineTo segment.", path.c_str());
                    continue;
                }

                curX = MathParser::ParseDouble(segmentList.at(0));
                curY = MathParser::ParseDouble(segmentList.at(1));
                drawList.push_back(Magick::PathLinetoAbs(Magick::Coordinate(
                    curX,
                    curY
                )));
            }
            else if (_wcsicmp(name, L"ARCTO") == 0)
            {
                if (segmentSize < 2)
                {
                    RmLogF(rm, LOG_WARNING, L"%s is invalid ArcTo segment.", path.c_str());
                    continue;
                }
                const double x = MathParser::ParseDouble(segmentList.at(0));
                const double y = MathParser::ParseDouble(segmentList.at(1));
                const double dx = x - curX;
                const double dy = y - curY;
                double xRadius = std::sqrt(dx * dx + dy * dy) / 2.0;
                double angle = 0.0;
                bool sweep = false;
                bool size = false;

                if (segmentSize > 2)
                    xRadius = MathParser::ParseDouble(segmentList.at(2));

                double yRadius = xRadius;

                if (segmentSize > 3)
                {
                    yRadius = Utils::ParseNumber(
                        yRadius,
                        segmentList.at(3).c_str(),
                        MathParser::ParseDouble
                    );
                }

                if (segmentSize > 4)
                {
                    angle = Utils::ParseNumber(
                        angle,
                        segmentList.at(4).c_str(),
                        MathParser::ParseDouble
                    );
                }

                if (segmentSize > 5)
                    Utils::ParseBool(sweep, segmentList.at(5));
                if (segmentSize > 6)
                    Utils::ParseBool(size, segmentList.at(6));

                drawList.push_back(Magick::PathArcAbs(Magick::PathArcArgs(
                    xRadius, yRadius,
                    angle, size, !sweep,
                    x, y
                )));

                curX = x;
                curY = y;
            }
            else if (_wcsicmp(name, L"CURVETO") == 0)
            {
                if (segmentSize < 4)
                {
                    RmLogF(rm, LOG_WARNING, L"%s is invalid CurveTo segment.", path.c_str());
                    continue;
                }
                const double x = MathParser::ParseDouble(segmentList.at(0));
                const double y = MathParser::ParseDouble(segmentList.at(1));
                const double cx1 = MathParser::ParseDouble(segmentList.at(2));
                const double cy1 = MathParser::ParseDouble(segmentList.at(3));
                if (segmentSize < 6)
                {
                    drawList.push_back(Magick::PathQuadraticCurvetoAbs(Magick::PathQuadraticCurvetoArgs(
                        cx1, cy1, x, y
                    )));
                }
                else
                {
                    const double cx2 = MathParser::ParseDouble(segmentList.at(4));
                    const double cy2 = MathParser::ParseDouble(segmentList.at(5));
                    drawList.push_back(Magick::PathCurvetoAbs(Magick::PathCurvetoArgs(
                        cx1, cy1, cx2, cy2, x, y
                    )));
                }
                curX = x;
                curY = y;
            }
            else if (_wcsicmp(name, L"SMOOTHCURVETO") == 0)
            {
                if (segmentSize < 2)
                {
                    RmLogF(rm, LOG_WARNING, L"%s is invalid SmoothCurveTo segment.", path.c_str());
                    continue;
                }
                const double x = MathParser::ParseDouble(segmentList.at(0));
                const double y = MathParser::ParseDouble(segmentList.at(1));

                if (segmentSize > 3)
                {
                    const double cx = MathParser::ParseDouble(segmentList.at(2));
                    const double cy = MathParser::ParseDouble(segmentList.at(3));

                    drawList.push_back(PathCubicSmoothCurvetoAbs(Magick::PathQuadraticCurvetoArgs(
                        cx, cy, x, y
                    )));
                }
                else
                {
                    drawList.push_back(Magick::PathSmoothQuadraticCurvetoAbs(Magick::Coordinate(
                        x, y
                    )));
                }
                curX = x;
                curY = y;
            }
            else if (_wcsicmp(name, L"CLOSEPATH") == 0)
            {
                if (MathParser::ParseBool(tempPara))
                    drawList.push_back(Magick::PathClosePath());
            }
            else
            {
                continue;
            }

            if (curX < imgX1) imgX1 = curX;
            if (curY < imgY1) imgY1 = curY;
            if (curX > imgX2) imgX2 = curX;
            if (curY > imgY2) imgY2 = curY;
        }

        strokeWidth = 2;

        outShape = new Shape{
            Magick::DrawablePath(drawList),
            Magick::DrawableList{ Magick::DrawableFillColor(INVISIBLE) },
            imgX2,
            imgY2
        };

        out.X = (ssize_t)ceil(imgX1);
        out.Y = (ssize_t)ceil(imgY1);
        out.W = (size_t)ceil(imgX2 - imgX1);
        out.H = (size_t)ceil(imgY2 - imgY1);
        break;
	}
	}

    if (outShape == nullptr)
    {
        return FALSE;
    }

	for (auto &option : config)
	{
        if (option.empty())
            continue;

		std::wstring tempName, tempPara;
		Utils::GetNamePara(option, tempName, tempPara);
		ParseInternalVariable(tempPara, out);

		LPCWSTR name = tempName.c_str();

		BOOL isSetting = FALSE;
		if (_wcsicmp(name, L"CANVAS") == 0)
		{
			WSVector valList = Utils::SeparateList(tempPara, L",", 2);
            const size_t tempW = MathParser::ParseSizeT(valList.at(0));
            const size_t tempH = MathParser::ParseSizeT(valList.at(1));

            if (tempW <= 0 || tempH <= 0)
            {
                RmLog(rm, LOG_WARNING, L"Invalid Width or Height value. Default canvas is used.");
            }
			else
			{
				customCanvas = Magick::Geometry(tempW, tempH);
				customCanvas.aspect(true);
			}
			isSetting = TRUE;
		}
		else if (_wcsicmp(name, L"OFFSET") == 0)
		{
			WSVector valList = Utils::SeparateParameter(tempPara, 2);
			outShape->attributes.push_back(Magick::DrawableTranslation(
				MathParser::ParseDouble(valList.at(0)),
				MathParser::ParseDouble(valList.at(1))
			));
			isSetting = TRUE;
		}
		else if (_wcsicmp(name, L"ROTATE") == 0)
		{
			outShape->attributes.push_back(Magick::DrawableRotation(
				MathParser::ParseDouble(tempPara)
			));
			isSetting = TRUE;
		}
		else if (_wcsicmp(name, L"COLOR") == 0)
		{
			outShape->attributes.push_back(Magick::DrawableFillColor(
                Utils::ParseColor(tempPara)
            ));
			isSetting = TRUE;
		}
		else if (_wcsicmp(name, L"FILLPATTERN") == 0)
		{
			const int index = Utils::NameToIndex(tempPara);
			if (index >= 0 && index < imgList.size())
				out.img.fillPattern(imgList.at(index).img);
		}
		else if (_wcsicmp(name, L"ANTIALIAS") == 0)
		{
			outShape->attributes.push_back(Magick::DrawableStrokeAntialias(
                MathParser::ParseBool(tempPara)
            ));
			isSetting = TRUE;
		}
		if (_wcsicmp(name, L"STROKECOLOR") == 0)
		{
			strokeColor = Magick::DrawableStrokeColor(
                Utils::ParseColor(tempPara)
            );
			isSetting = TRUE;
		}
		else if (_wcsicmp(name, L"STROKEWIDTH") == 0)
		{
			strokeWidth = abs(MathParser::ParseDouble(tempPara));
			outShape->attributes.push_back(Magick::DrawableStrokeWidth(strokeWidth));
			isSetting = TRUE;
		}
		else if (_wcsicmp(name, L"STROKEALIGN") == 0)
		{
			if (_wcsicmp(tempPara.c_str(), L"CENTER") == 0)
				strokeAlign = 0;

			else if (_wcsicmp(tempPara.c_str(), L"OUTSIDE") == 0)
				strokeAlign = 1;

			else if (_wcsicmp(tempPara.c_str(), L"INSIDE") == 0)
				strokeAlign = 2;
			else
				RmLog(rm, LOG_WARNING, L"Invalid StrokeAlign value. Center align is applied.");

			isSetting = TRUE;
		}
		else if (_wcsicmp(name, L"STROKELINEJOIN") == 0)
		{
			Magick::LineJoin lj = MagickCore::MiterJoin;
			if (_wcsicmp(tempPara.c_str(), L"MITER") == 0)
				lj = MagickCore::MiterJoin;
			else if (_wcsicmp(tempPara.c_str(), L"ROUND") == 0)
				lj = MagickCore::RoundJoin;
			else if (_wcsicmp(tempPara.c_str(), L"BEVEL") == 0)
				lj = MagickCore::BevelJoin;
			else
			{
				RmLog(rm, LOG_WARNING, L"Invalid StrokeLineJoin value. Miter line join is applied.");
			}
			outShape->attributes.push_back(Magick::DrawableStrokeLineJoin(lj));
		}
		else if (_wcsicmp(name, L"STROKELINECAP") == 0)
		{
			Magick::LineCap lc = MagickCore::ButtCap;;
			if (_wcsicmp(tempPara.c_str(), L"FLAT") == 0)
				lc = MagickCore::ButtCap;
			else if (_wcsicmp(tempPara.c_str(), L"ROUND") == 0)
				lc = MagickCore::RoundCap;
			else if (_wcsicmp(tempPara.c_str(), L"SQUARE") == 0)
				lc = MagickCore::SquareCap;
			else
			{
				RmLog(rm, LOG_WARNING, L"Invalid StrokeLineCap value. Flat line cap is applied.");
			}
			outShape->attributes.push_back(Magick::DrawableStrokeLineCap(lc));
		}

		if (isSetting)
			option.clear();
	}

	try
	{
		if (!customCanvas.isValid())
		{
            customCanvas = Magick::Geometry(
                (size_t)(outShape->canvasWidth + strokeWidth),
                (size_t)(outShape->canvasHeight + strokeWidth)
            );
            customCanvas.aspect(true);
		}

        out.img.scale(customCanvas);

		if (strokeWidth == 0)
		{
			outShape->attributes.push_back(Magick::DrawableStrokeColor(INVISIBLE));
            outShape->attributes.push_back(outShape->shape);
            out.img.draw(outShape->attributes);
		}
		else if (strokeWidth != 0 && strokeAlign != 0)
		{
			outShape->attributes.push_back(strokeColor);
			Magick::Image temp = out.img;
			const double strokeWidth2 = strokeWidth * 2;
            auto doubleWidth = outShape->attributes;
            doubleWidth.push_back(Magick::DrawableStrokeWidth(strokeWidth2));
            doubleWidth.push_back(outShape->shape);
            out.img.draw(doubleWidth);

            auto noWidth = outShape->attributes;
            noWidth.push_back(Magick::DrawableStrokeWidth(0));
            noWidth.push_back(outShape->shape);
            temp.draw(noWidth);

			if (strokeAlign == 1)
			{
				out.X -= (ssize_t)ceil(strokeWidth);
				out.Y -= (ssize_t)ceil(strokeWidth);
				out.W += (size_t)ceil(strokeWidth2);
				out.H += (size_t)ceil(strokeWidth2);
				out.img.composite(temp, 0, 0, Magick::OverCompositeOp);
			}
			else if (strokeAlign == 2)
			{
				out.img.composite(temp, 0, 0, Magick::CopyAlphaCompositeOp);
			}
		}
		else
		{
			out.X -= (ssize_t)ceil(strokeWidth / 2);
			out.Y -= (ssize_t)ceil(strokeWidth / 2);
			out.W += (size_t)ceil(strokeWidth);
			out.H += (size_t)ceil(strokeWidth);
			outShape->attributes.push_back(strokeColor);
            outShape->attributes.push_back(outShape->shape);
            out.img.draw(outShape->attributes);
		}

        delete outShape;
	}
	catch (Magick::Exception &error_)
	{
		LogError(error_);
		return FALSE;
	}

	return TRUE;
}

void DrawRoundedCorner(
    Magick::VPathList &drawList,
    const Magick::Coordinate &angularPoint,
	const Magick::Coordinate &p1,
    const Magick::Coordinate &p2,
    double radius,
    bool start)
{
	// Vector 1
	const double dx1 = angularPoint.x() - p1.x();
	const double dy1 = angularPoint.y() - p1.y();

	// Vector 2
    const double dx2 = angularPoint.x() - p2.x();
    const double dy2 = angularPoint.y() - p2.y();

	// Angle between vector 1 and vector 2 divided by 2
    const double angle = (atan2(dy1, dx1) - atan2(dy2, dx2)) / 2;

	// The length of segment between angular point and the
	// points of intersection with the circle of a given radius
    const double tanR = abs(tan(angle));
	double segment = radius / tanR;

	// Check the segment
    const double length1 = GetLength(dx1, dy1);
    const double length2 = GetLength(dx2, dy2);

    const double length = min(length1, length2) / 2;

	if (segment > length)
	{
		segment = length;
		radius = length * tanR;
	}

	auto p1Cross = GetProportionPoint(angularPoint, segment, length1, dx1, dy1);
	auto p2Cross = GetProportionPoint(angularPoint, segment, length2, dx2, dy2);

	if (start) drawList.push_back(Magick::PathMovetoAbs(p1Cross));
    drawList.push_back(Magick::PathLinetoAbs(p1Cross));

    const double diameter = 2 * radius;
    const double degreeFactor = 180 / MagickPI;

    drawList.push_back(Magick::PathArcAbs(Magick::PathArcArgs(
		radius, radius, 0, false, false, p2Cross.x(), p2Cross.y())));
}

double GetLength(double dx, double dy) noexcept
{
	return sqrt(dx * dx + dy * dy);
}

Magick::Coordinate GetProportionPoint(Magick::Coordinate point, double segment,
	double length, double dx, double dy)
{
	const double factor = segment / length;

	return Magick::Coordinate(
		point.x() - dx * factor,
		point.y() - dy * factor
	);
}

Shape* CreateEllipse(WSVector &inParaList, ImgContainer &outImg)
{
    const size_t pSize = inParaList.size();
    const double x = MathParser::ParseDouble(inParaList.at(0));
    const double y = MathParser::ParseDouble(inParaList.at(1));
    const double radiusX = MathParser::ParseDouble(inParaList.at(2));

    double start = 0;
    double end = 360;
    double radiusY = radiusX;

    if (pSize > 3) radiusY = MathParser::ParseDouble(inParaList.at(3));

    if (pSize > 4) start = MathParser::ParseDouble(inParaList.at(4));

    if (pSize > 5) end = MathParser::ParseDouble(inParaList.at(5));

    outImg.X = static_cast<ssize_t>(x - abs(radiusX));
    outImg.Y = static_cast<ssize_t>(y - abs(radiusY));
    outImg.W = static_cast<size_t>(radiusX * 2.0);
    outImg.H = static_cast<size_t>(radiusY * 2.0);

    return new Shape{
        Magick::DrawableEllipse(x, y, radiusX, radiusY, start, end),
        Magick::DrawableList{ Magick::DrawableFillColor(Magick::Color("white")) },
        x + abs(radiusX) + 1,
        y + abs(radiusY) + 1,
    };
}

Shape* CreateRectangle(WSVector &inParaList, ImgContainer &outImg)
{
    const size_t pSize = inParaList.size();

    double x = MathParser::ParseDouble(inParaList.at(0));
    double y = MathParser::ParseDouble(inParaList.at(1));
    double w = MathParser::ParseDouble(inParaList.at(2));
    double h = MathParser::ParseDouble(inParaList.at(3));

    if (w < 0)
    {
        x += w;
        w = abs(w);
    }

    if (h < 0)
    {
        y += h;
        h = abs(h);
    }

    double cornerX = 0.0;
    if (pSize > 4) cornerX = MathParser::ParseDouble(inParaList.at(4));

    double cornerY = cornerX;
    if (pSize > 5) cornerY = MathParser::ParseDouble(inParaList.at(5));

    Magick::Drawable outShape;
    if (cornerX == 0 && cornerY == 0)
        outShape = Magick::DrawableRectangle(x, y, x + w, y + h);
    else
        outShape = Magick::DrawableRoundRectangle(x, y, x + w, y + h, cornerX, cornerY);

    double outWidth = 0.0;

    outImg.X = static_cast<ssize_t>(x);
    outImg.Y = static_cast<ssize_t>(y);
    outImg.W = static_cast<size_t>(w);
    outImg.H = static_cast<size_t>(h);

    return new Shape{
        outShape,
        Magick::DrawableList{ Magick::DrawableFillColor(Magick::Color("white")) },
        w + x + 1,
        h + y + 1
    };
}

Shape* CreateRectangleCustomCorner(WSVector &inParaList, std::vector<double> &inCornerList, ImgContainer &outImg)
{
    double x = MathParser::ParseDouble(inParaList.at(0));
    double y = MathParser::ParseDouble(inParaList.at(1));
    double w = MathParser::ParseDouble(inParaList.at(2));
    double h = MathParser::ParseDouble(inParaList.at(3));

    if (w < 0)
    {
        x += w;
        w = abs(w);
    }

    if (h < 0)
    {
        y += h;
        h = abs(h);
    }

    Magick::VPathList pathList;

    Magick::Coordinate point1(x + w, y);
    Magick::Coordinate angularPoint(x, y);
    Magick::Coordinate point2(x, y + h);
    DrawRoundedCorner(pathList, angularPoint, point1, point2, inCornerList.at(0), true);

    point1 = angularPoint;
    angularPoint = point2;
    point2 = Magick::Coordinate(x + w, y + h);
    DrawRoundedCorner(pathList, angularPoint, point1, point2, inCornerList.at(3), false);

    point1 = angularPoint;
    angularPoint = point2;
    point2 = Magick::Coordinate(x + w, y);
    DrawRoundedCorner(pathList, angularPoint, point1, point2, inCornerList.at(2), false);

    point1 = angularPoint;
    angularPoint = point2;
    point2 = Magick::Coordinate(x, y);
    DrawRoundedCorner(pathList, angularPoint, point1, point2, inCornerList.at(1), false);

    pathList.push_back(Magick::PathClosePath());

    outImg.X = (ssize_t)x;
    outImg.Y = (ssize_t)y;
    outImg.W = (size_t)abs(w);
    outImg.H = (size_t)abs(h);

    return new Shape{
        Magick::DrawablePath(pathList),
        Magick::DrawableList{ Magick::DrawableFillColor(Magick::Color("white")) },
        w + x + 1,
        h + y + 1
    };
}

Shape* CreatePolygon(WSVector &inParaList, std::vector<double> &inCornerList, ImgContainer &outImg)
{
    const size_t pSize = inParaList.size();
    const double origX = MathParser::ParseDouble(inParaList.at(0));
    const double origY = MathParser::ParseDouble(inParaList.at(1));
    const double side = round(MathParser::ParseDouble(inParaList.at(2)));
    const double radiusX = abs(MathParser::ParseDouble(inParaList.at(3)));
    double radiusY = radiusX;
    if (pSize > 4)
    {
        // Allow user to skip radiusY parameter
        radiusY = Utils::ParseNumber(
            radiusY,
            inParaList.at(4).c_str(),
            MathParser::ParseDouble
        );
    }
    double startAngle = 0;
    if (pSize > 5) startAngle = MathParser::ParseDouble(inParaList.at(5)) * MagickPI / 180;

    Magick::VPathList pathList;
    if (inCornerList.size() > 0)
    {
        const double startRad = MagickPI2 + startAngle;
        Magick::Coordinate point1(
            origX + radiusX * cos(startRad),
            origY - radiusY * sin(startRad)
        );

        const double factor = 1 / side;

        const double angPointRad = factor * Magick2PI + startRad;
        Magick::Coordinate angularPoint(
            origX + radiusX * cos(angPointRad),
            origY - radiusY * sin(angPointRad)
        );

        for (double i = 1; i <= side; i++)
        {
            const double rad = (i + 1.0) * factor * Magick2PI + startRad;
            Magick::Coordinate point2(
                origX + radiusX * cos(rad),
                origY - radiusY * sin(rad)
            );

            const size_t cornerIndex = (size_t)(side - (i - 1)) % (size_t)side;

            DrawRoundedCorner(
                pathList, angularPoint, point1, point2,
                inCornerList.at(cornerIndex),
                i == 1
            );

            point1 = angularPoint;
            angularPoint = point2;
        }
    }
    else
    {
        for (double i = 0; i < side; i++)
        {
            const double rad = (i / side) * Magick2PI + MagickPI2 + startAngle;
            const double curX = origX + radiusX * cos(rad);
            const double curY = origY - radiusY * sin(rad);

            if (i == 0)
            {
                pathList.push_back(Magick::PathMovetoAbs(Magick::Coordinate(
                    curX, curY
                )));
            }
            else
            {
                pathList.push_back(Magick::PathLinetoAbs(Magick::Coordinate(
                    curX, curY
                )));
            }
        }
    }

    pathList.push_back(Magick::PathClosePath());

    outImg.X = (ssize_t)round(origX - radiusX);
    outImg.Y = (ssize_t)round(origY - radiusY);
    outImg.W = (size_t)ceil(radiusX * 2);
    outImg.H = (size_t)ceil(radiusY * 2);

    return new Shape{
        Magick::DrawablePath(pathList),
        Magick::DrawableList{ Magick::DrawableFillColor(Magick::Color("white")) },
        origX + radiusX + 1,
        origY + radiusY + 1
    };
}
