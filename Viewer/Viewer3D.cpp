#include "Viewer3D.h"

#include <vtkRenderWindow.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkImageImport.h>
#include <vtkFixedPointVolumeRayCastMapper.h>
#include <vtkColorTransferFunction.h>
#include <vtkVolumeProperty.h>
#include <vtkPiecewiseFunction.h>
#include <vtkMarchingCubes.h>
#include <vtkStripper.h>
#include <vtkPolyDataConnectivityFilter.h>
#include <vtkNew.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkAxesActor.h>
#include <vtkCaptionActor2D.h>
#include <vtkProperty2D.h>
#include <vtkTextProperty.h>
#include <vtkTextActor.h>
#include <vtkImageActor.h>
#include <vtkImageMapper3D.h>
#include <vtkImageMapper.h>
#include <vtkTransform.h>
#include <vtkLookupTable.h>
#include <vtkFloatArray.h>
#include <vtkCellData.h>

Viewer3D::Viewer3D(QWidget *parent) : QVTKWidget(parent) {
	ui.setupUi(this);

	this->renderer = vtkSmartPointer<vtkRenderer>::New();
	this->renderer->SetBackground(.2, .2, .2);
	this->renderer->SetUseDepthPeeling(true);
	this->GetRenderWindow()->AddRenderer(this->renderer);
	this->GetRenderWindow()->Render();

	this->mouse_style = vtkSmartPointer<MouseInteractorStyle>::New();
	this->mouse_style->SetDefaultRenderer(this->renderer);
	this->GetInteractor()->SetInteractorStyle(this->mouse_style);
}

Viewer3D::~Viewer3D() {
}

void Viewer3D::updateView() {
	this->renderer->RemoveAllViewProps();

	if (renderingMode == VOLUME_RENDERING) {
		for (int i = 0; i < volumes.size(); ++i) {
			if (!visible[i])
				continue;

			vtkSmartPointer<vtkImageImport> image_import = vtkSmartPointer<vtkImageImport>::New();
			image_import->SetDataSpacing(volumes[i].dx, volumes[i].dy, volumes[i].dz);
			image_import->SetDataOrigin(0, 0, 0);
			image_import->SetWholeExtent(0, volumes[i].nx - 1, 0, volumes[i].ny - 1, 0, volumes[i].nz - 1);
			image_import->SetDataExtentToWholeExtent();
			image_import->SetDataScalarTypeToUnsignedShort();
			image_import->SetNumberOfScalarComponents(1);
			image_import->SetImportVoidPointer(volumes[i].data);
			image_import->Update();

			vtkSmartPointer<vtkFixedPointVolumeRayCastMapper> volumeMapper = vtkSmartPointer<vtkFixedPointVolumeRayCastMapper>::New();
			volumeMapper->SetInputConnection(image_import->GetOutputPort());

			vtkSmartPointer<vtkColorTransferFunction> volumeColor = vtkSmartPointer<vtkColorTransferFunction>::New();
			volumeColor->AddRGBPoint(0, 0.0, 0.0, 0.0);
			volumeColor->AddRGBPoint(200, (double)color[i].red() / 255.0, 
				(double)color[i].green() / 255.0, (double)color[i].blue() / 255.0);

			vtkSmartPointer<vtkPiecewiseFunction> volumeScalarOpacity = vtkSmartPointer<vtkPiecewiseFunction>::New();
			volumeScalarOpacity->AddPoint(0, 0.0);
			volumeScalarOpacity->AddPoint(200, (double)color[i].alpha() / 255.0);

			vtkSmartPointer<vtkPiecewiseFunction> volumeGradientOpacity = vtkSmartPointer<vtkPiecewiseFunction>::New();
			volumeGradientOpacity->AddPoint(0, 0.0);
			volumeGradientOpacity->AddPoint(200, 1.0);

			vtkSmartPointer<vtkVolumeProperty> volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();
			volumeProperty->SetColor(volumeColor);
			volumeProperty->SetScalarOpacity(volumeScalarOpacity);
			volumeProperty->SetGradientOpacity(volumeGradientOpacity);
			volumeProperty->SetInterpolationTypeToLinear();
			volumeProperty->ShadeOn();
			volumeProperty->SetAmbient(ambient);
			volumeProperty->SetDiffuse(diffuse);
			volumeProperty->SetSpecular(specular);

			vtkSmartPointer<vtkVolume> volumeVTK = vtkSmartPointer<vtkVolume>::New();
			volumeVTK->SetMapper(volumeMapper);
			volumeVTK->SetProperty(volumeProperty);
			this->renderer->AddViewProp(volumeVTK);
		}
	} else if (renderingMode == MESH_RENDERING) {
		for (int i = 0; i < meshes.size(); ++i) {
			if (!visible[i])
				continue;

			vtkSmartPointer<vtkPolyDataMapper> meshMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
			meshMapper->SetInputData(meshes[i]);
			meshMapper->ScalarVisibilityOff();

			vtkSmartPointer<vtkActor> meshActor = vtkSmartPointer<vtkActor>::New();
			meshActor->SetMapper(meshMapper);
			meshActor->GetProperty()->SetColor((double)color[i].red() / 255.0, (double)color[i].green() / 255.0, (double)color[i].blue() / 255.0);
			meshActor->GetProperty()->SetOpacity((double)color[i].alpha() / 255.0);
			meshActor->GetProperty()->SetAmbient(ambient);
			meshActor->GetProperty()->SetDiffuse(diffuse);
			meshActor->GetProperty()->SetSpecular(specular);

			this->renderer->AddActor(meshActor);
		}
	}

	if (volumes.size() && axesFlag) {
		vtkSmartPointer<vtkAxesActor> axes = vtkSmartPointer<vtkAxesActor>::New();
		axes->SetPosition(0, 0, 0);
		double maxLen = lenX > lenY ? (lenX > lenZ ? lenX : lenZ) : (lenY > lenZ ? lenY : lenZ);
		axes->SetTotalLength(maxLen / 10.0, maxLen / 10.0, maxLen / 10.0);
		axes->GetXAxisCaptionActor2D()->GetTextActor()->SetTextScaleMode(vtkTextActor::TEXT_SCALE_MODE_NONE);
		axes->GetXAxisCaptionActor2D()->GetTextActor()->GetTextProperty()->SetFontSize(maxLen / 15);
		axes->GetYAxisCaptionActor2D()->GetTextActor()->SetTextScaleMode(vtkTextActor::TEXT_SCALE_MODE_NONE);
		axes->GetYAxisCaptionActor2D()->GetTextActor()->GetTextProperty()->SetFontSize(maxLen / 15);
		axes->GetZAxisCaptionActor2D()->GetTextActor()->SetTextScaleMode(vtkTextActor::TEXT_SCALE_MODE_NONE);
		axes->GetZAxisCaptionActor2D()->GetTextActor()->GetTextProperty()->SetFontSize(maxLen / 15);

		this->renderer->AddActor(axes);
	}

	if (volumes.size() && sliceFlag) {
		vtkSmartPointer<vtkTransform> transform_saggital = vtkSmartPointer<vtkTransform>::New();
		transform_saggital->Identity();
		transform_saggital->RotateY(90.0);
		transform_saggital->RotateZ(90.0);
		transform_saggital->Translate(0, 0, slicePos[0]);

		vtkSmartPointer<vtkTransform> transform_coronal = vtkSmartPointer<vtkTransform>::New();
		transform_coronal->Identity();
		transform_coronal->RotateX(90.0);
		transform_coronal->Translate(0, 0, -slicePos[1]);

		vtkSmartPointer<vtkTransform> transform_transverse = vtkSmartPointer<vtkTransform>::New();
		transform_transverse->Identity();
		transform_transverse->Translate(0, 0, slicePos[2]);

		vtkSmartPointer<vtkImageActor> actor_saggital = vtkSmartPointer<vtkImageActor>::New();
		actor_saggital->GetMapper()->SetInputData(generateSlice2d(SAGITTAL_PLANE, slicePos[0]));
		actor_saggital->SetUserTransform(transform_saggital);

		vtkSmartPointer<vtkImageActor> actor_coronal = vtkSmartPointer<vtkImageActor>::New();
		actor_coronal->GetMapper()->SetInputData(generateSlice2d(CORONAL_PLANE, slicePos[1]));
		actor_coronal->SetUserTransform(transform_coronal);

		vtkSmartPointer<vtkImageActor> actor_transverse = vtkSmartPointer<vtkImageActor>::New();
		actor_transverse->GetMapper()->SetInputData(generateSlice2d(TRANSVERSE_PLANE, slicePos[2]));
		actor_transverse->SetUserTransform(transform_transverse);

		this->renderer->AddActor(actor_saggital);
		this->renderer->AddActor(actor_coronal);
		this->renderer->AddActor(actor_transverse);
	}

	for (int i = 0; i < dsaVisible.size(); ++i) {
		if (!dsaVisible[i] || dsaFrames[i] < 0 || dsaFrames[i] >= dsaImages[i].nz)
			continue;

		vtkSmartPointer<vtkImageData> dsaImage = vtkSmartPointer<vtkImageData>::New();
		dsaImage->SetDimensions(dsaImages[i].nx, dsaImages[i].ny, 1);
		dsaImage->AllocateScalars(VTK_UNSIGNED_CHAR, 3);
		int basePtr = dsaFrames[i] * dsaImages[i].nx * dsaImages[i].ny;
		for (unsigned int x = 0; x < dsaImages[i].nx; x++) {
			for (unsigned int y = 0; y < dsaImages[i].ny; y++) {
				unsigned char* pixel = static_cast<unsigned char*>(dsaImage->GetScalarPointer(x, y, 0));
				pixel[0] = pixel[1] = pixel[2] = dsaImages[i].data[basePtr + (dsaImages[i].ny - y - 1) * dsaImages[i].nx + x];
			}
		}
		dsaImage->Modified();

		vtkSmartPointer<vtkImageMapper> imageMapper = vtkSmartPointer<vtkImageMapper>::New();
		imageMapper->SetInputData(dsaImage);
		imageMapper->SetColorWindow(255);
		imageMapper->SetColorLevel(127.5);

		vtkSmartPointer<vtkActor2D> actor_dsa = vtkSmartPointer<vtkActor2D>::New();
		actor_dsa->SetMapper(imageMapper);
		actor_dsa->SetPosition(0, 0);
		actor_dsa->GetProperty()->SetOpacity(0.1);
		actor_dsa->GetProperty()->SetDisplayLocationToBackground();

		this->renderer->AddActor2D(actor_dsa);
	}

	if (isFirstRead && volumes.size()) {
		this->renderer->ResetCamera();
		isFirstRead = false;
	}
	this->GetRenderWindow()->Render();
	this->update();
}

void Viewer3D::addVolume(VolumeData<short> v, QString title) {
	this->title.push_back(title);
	volumes.push_back(v);
	isoValue.push_back(200);
	meshes.push_back(isoSurface(v, 200));
	WindowCenter.push_back(200);
	WindowWidth.push_back(800);
	color.push_back(QColor(255, 255, 255, 255));
	visible.push_back(true);

	lenX = (v.dx * v.nx) > lenX ? (v.dx * v.nx) : lenX;
	lenY = (v.dy * v.ny) > lenY ? (v.dy * v.ny) : lenY;
	lenZ = (v.dz * v.nz) > lenZ ? (v.dz * v.nz) : lenZ;
}

void Viewer3D::deleteVolume(int idx) {
	if (idx < 0 || idx >= volumes.size())
		return;
	volumes.erase(volumes.begin() + idx);
	meshes.erase(meshes.begin() + idx);
	visible.erase(visible.begin() + idx);
	isoValue.erase(isoValue.begin() + idx);
	WindowWidth.erase(WindowWidth.begin() + idx);
	WindowCenter.erase(WindowCenter.begin() + idx);
	color.erase(color.begin() + idx);
	title.erase(title.begin() + idx);
}

void Viewer3D::addDSAImage(VolumeData<short> v, QString title) {
	dsaImages.push_back(v);
	dsaTitles.push_back(title);
	dsaVisible.push_back(true);
	dsaFrames.push_back(0);
}

vtkSmartPointer<vtkPolyData> Viewer3D::isoSurface(VolumeData<short> &v, int isoValue, bool skipConnectivityFilter) {
	vtkSmartPointer<vtkImageImport> image_import = vtkSmartPointer<vtkImageImport>::New();
	image_import->SetDataSpacing(v.dx, v.dy, v.dz);
	image_import->SetDataOrigin(0, 0, 0);
	image_import->SetWholeExtent(0, v.nx - 1, 0, v.ny - 1, 0, v.nz - 1);
	image_import->SetDataExtentToWholeExtent();
	image_import->SetDataScalarTypeToShort();
	image_import->SetNumberOfScalarComponents(1);
	image_import->SetImportVoidPointer(v.data);
	image_import->Update();

	vtkSmartPointer<vtkMarchingCubes> marchingCubes = vtkSmartPointer<vtkMarchingCubes>::New();
	marchingCubes->SetInputConnection(image_import->GetOutputPort());
	marchingCubes->SetValue(0, isoValue);
	marchingCubes->Update();

	vtkSmartPointer<vtkStripper> stripper = vtkSmartPointer<vtkStripper>::New();
	stripper->SetInputConnection(marchingCubes->GetOutputPort());
	stripper->Update();

	if (skipConnectivityFilter) {
		return stripper->GetOutput();
	}

	vtkSmartPointer<vtkPolyDataConnectivityFilter> connectivityFilter = vtkSmartPointer<vtkPolyDataConnectivityFilter>::New();
	connectivityFilter->SetInputConnection(stripper->GetOutputPort());
	connectivityFilter->SetExtractionModeToLargestRegion();
	connectivityFilter->Update();

	if (connectivityFilter->GetOutput()->GetNumberOfCells() <= 0) {
		return connectivityFilter->GetOutput();
	}

	vtkNew<vtkSmoothPolyDataFilter> smoothFilter;
	smoothFilter->SetInputData(connectivityFilter->GetOutput());
	smoothFilter->SetNumberOfIterations(15);
	smoothFilter->SetRelaxationFactor(0.1);
	smoothFilter->Update();

	return smoothFilter->GetOutput();
}

cv::Mat Viewer3D::generateSlice2d(int plane, double pos, int scale) {
	if (lenX == 0 || lenY == 0 || lenZ == 0) {
		cv::Mat blank_img(scale, scale, CV_8UC3);
		for (int i = 0; i < scale; ++i) for (int j = 0; j < scale; ++j)
			blank_img.at<cv::Vec3b>(i, j)[0] = blank_img.at<cv::Vec3b>(i, j)[1] = blank_img.at<cv::Vec3b>(i, j)[2] = 0;
		return blank_img;
	}
	int xMax = 0, yMax = 0;
	double spacing = 1.0;
	if (plane == SAGITTAL_PLANE) {
		if (lenZ > lenY) {
			xMax = (double)scale * lenY / lenZ;
			yMax = (double)scale;
			spacing = (double)lenZ / scale;
		} else {
			xMax = (double)scale;
			yMax = (double)scale * lenZ / lenY;
			spacing = (double)lenY / scale;
		}
	} else if (plane == CORONAL_PLANE) {
		if (lenZ > lenX) {
			xMax = (double)scale * lenX / lenZ;
			yMax = (double)scale;
			spacing = (double)lenZ / scale;
		} else {
			xMax = (double)scale;
			yMax = (double)scale * lenZ / lenX;
			spacing = (double)lenX / scale;
		}
	} else {
		if (lenY > lenX) {
			xMax = (double)scale * lenX / lenY;
			yMax = (double)scale;
			spacing = (double)lenY / scale;
		} else {
			xMax = (double)scale;
			yMax = (double)scale * lenY / lenX;
			spacing = (double)lenX / scale;
		}
	}
	cv::Mat img(yMax, xMax, CV_8UC3);
	for (int y = 0; y < yMax; ++y) {
		for (int x = 0; x < xMax; ++x) {
			int r = 0, g = 0, b = 0;
			for (int i = 0; i < volumes.size(); ++i) {
				if (!visible[i])
					continue;
				double xIndex = 0, yIndex = 0, zIndex = 0;
				if (plane == SAGITTAL_PLANE) {
					xIndex = (double)pos / volumes[i].dx;
					yIndex = (double)spacing * x / volumes[i].dy;
					zIndex = (double)spacing * y / volumes[i].dz;
				} else if (plane == CORONAL_PLANE) {
					xIndex = (double)spacing * x / volumes[i].dx;
					yIndex = (double)pos / volumes[i].dy;
					zIndex = (double)spacing * y / volumes[i].dz;
				} else {
					xIndex = (double)spacing * x / volumes[i].dx;
					yIndex = (double)spacing * y / volumes[i].dy;
					zIndex = (double)pos / volumes[i].dz;
				}
				if (xIndex >= volumes[i].nx || yIndex >= volumes[i].ny || zIndex >= volumes[i].nz)
					continue;
				short val = volumes[i].trilinear(xIndex, yIndex, zIndex);
				double op = (double)(val - WindowCenter[i] + WindowWidth[i] / 2.0) / WindowWidth[i];
				op = op < 0 ? 0 : op;
				op = op > 1 ? 1 : op;
				r += op * color[i].red();
				g += op * color[i].green();
				b += op * color[i].blue();
			}
			r = r > 255 ? 255 : r;
			g = g > 255 ? 255 : g;
			b = b > 255 ? 255 : b;
			if (plane == TRANSVERSE_PLANE) {
				img.at<cv::Vec3b>(y, x)[0] = r;
				img.at<cv::Vec3b>(y, x)[1] = g;
				img.at<cv::Vec3b>(y, x)[2] = b;
			} else {
				img.at<cv::Vec3b>(yMax - y - 1, x)[0] = r;
				img.at<cv::Vec3b>(yMax - y - 1, x)[1] = g;
				img.at<cv::Vec3b>(yMax - y - 1, x)[2] = b;
			}
		}
	}
	return img;
}

vtkSmartPointer<vtkImageData> Viewer3D::generateSlice2d(int plane, double pos) {
	vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
	image->SetSpacing(1.0, 1.0, 1.0);
	image->SetOrigin(0.0, 0.0, 0.0);
	int nx, ny;
	switch (plane) {
	case SAGITTAL_PLANE:
		nx = lenY, ny = lenZ;
		break;
	case CORONAL_PLANE:
		nx = lenX, ny = lenZ;
		break;
	case TRANSVERSE_PLANE:
		nx = lenX, ny = lenY;
		break;
	}
	image->SetExtent(0, nx - 1, 0, ny - 1, 0, 0);
	image->SetDimensions(nx, ny, 1);
	image->AllocateScalars(VTK_UNSIGNED_CHAR, 3);
	for (int x = 0; x < nx; ++x) for (int y = 0; y < ny; ++y) {
		int r = 0, g = 0, b = 0;
		for (int i = 0; i < volumes.size(); ++i) {
			if (!visible[i])
				continue;
			double xIndex, yIndex, zIndex;
			switch (plane) {
			case SAGITTAL_PLANE:
				xIndex = (double)pos / volumes[i].dx;
				yIndex = (double)x / volumes[i].dy;
				zIndex = (double)y / volumes[i].dz;
				break;
			case CORONAL_PLANE:
				xIndex = (double)x / volumes[i].dx;
				yIndex = (double)pos / volumes[i].dy;
				zIndex = (double)y / volumes[i].dz;
				break;
			case TRANSVERSE_PLANE:
				xIndex = (double)x / volumes[i].dx;
				yIndex = (double)y / volumes[i].dy;
				zIndex = (double)pos / volumes[i].dz;
				break;
			}
			if (xIndex >= volumes[i].nx || yIndex >= volumes[i].ny || zIndex >= volumes[i].nz)
				continue;
			short val = volumes[i].trilinear(xIndex, yIndex, zIndex);
			double op = (double)(val - WindowCenter[i] + WindowWidth[i] / 2.0) / WindowWidth[i];
			op = op < 0 ? 0 : op;
			op = op > 1 ? 1 : op;
			r += op * color[i].red();
			g += op * color[i].green();
			b += op * color[i].blue();
		}
		r = r > 255 ? 255 : r;
		g = g > 255 ? 255 : g;
		b = b > 255 ? 255 : b;
		image->SetScalarComponentFromDouble(x, y, 0, 0, r);
		image->SetScalarComponentFromDouble(x, y, 0, 1, g);
		image->SetScalarComponentFromDouble(x, y, 0, 2, b);
	}
	image->Modified();
	return image;
}

void Viewer3D::setAmbient(int v) {
	ambient = (double)v / 100.0;
	updateView();
}

void Viewer3D::setDiffuse(int v) {
	diffuse = (double)v / 100.0;
	updateView();
}

void Viewer3D::setSpecular(int v) {
	specular = (double)v / 100.0;
	updateView();
}

void Viewer3D::setRenderMode(int v) {
	renderingMode = v ? VOLUME_RENDERING : MESH_RENDERING;
	updateView();
}

void Viewer3D::setVisible(int v) {
	visible[v] = !visible[v];
	updateView();
}

void Viewer3D::setIsoValue(int i, int v) {
	isoValue[i] = v;
	meshes[i] = isoSurface(volumes[i], v);
	updateView();
}
