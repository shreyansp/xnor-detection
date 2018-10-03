#include "framegrabber.h"
#include <QRandomGenerator>
#include <QDebug>

QVideoFilterRunnable *FrameGrabber::createFilterRunnable()
{
    return new FilterRunnable(this);
}

FilterRunnable::FilterRunnable(FrameGrabber *frameGrabber)
    : m_frameGrabber(frameGrabber)
{
}


QVideoFrame FilterRunnable::run(QVideoFrame *input, const QVideoSurfaceFormat &surfaceFormat, RunFlags flags){
    Q_UNUSED(surfaceFormat);
    Q_UNUSED(flags);

    input->map(QAbstractVideoBuffer::ReadOnly);

    QByteArray * imgBytes = new QByteArray((const char*)input->bits(), input->mappedBytes());

    xnor_model *xmodel = NULL;
    xnor_error *xerror = NULL; // error returned by the image handles
    xnor_input *xinput; // input handle
    xnor_evaluation_result *xresult;
    xnor_evaluation_result_type xtype;
    xnor_bounding_box bbox[MAX_OUT_SIZE];
    int32_t out_size;

    if(!imgBytes->isEmpty() && !imgBytes->isNull()){

        // pass byte array to XNOR.ai class


        /*** LOAD MODEL BUILT IN ***/
        xerror = xnor_model_load_built_in("", NULL, &xmodel);

        if(xerror != NULL){
            qDebug() << "xnor_model_load_built_in return error\n"
                     << xnor_error_get_description(xerror);
            return QVideoFrame();
        }
        qDebug() << "xnor_model_load_built_in successful!";


        /*** CREATE A HANDLE TO IMAGE INPUT ***/
        switch (input->pixelFormat()) {
        case QVideoFrame::Format_YUV420P:{
            // splits into planes
            uint8_t *y = (uint8_t *)input->bits(1); //
            uint8_t *u = (uint8_t *)input->bits(2); //
            uint8_t *v = (uint8_t *)input->bits(3); //
            xerror = xnor_input_create_yuv420p_image(input->width(), input->height(),y, u, v, &xinput);
            break;
        }
        case QVideoFrame::Format_RGB32:{
            xerror = xnor_input_create_rgb_image(input->width(), input->height(), (uint8_t *)imgBytes->data(), &xinput);
            break;
        }
        case QVideoFrame::Format_YUYV:{ // YUYV IS YUV422
           xerror = xnor_input_create_yuv422_image(input->width(), input->height(), (uint8_t *)imgBytes->data(), &xinput);
            break;
        }
        default:
            break;
        }

        if(xerror != NULL){
            qDebug() << "xnor_input_create_" << input->pixelFormat() << "image error!\n"
                     << xnor_error_get_description(xerror);
        }
        qDebug() << "xnor_input_create_" << input->pixelFormat() << "image successful!";

        /*** EVALUATES THE GIVEN MODEL ON THE GIVEN INPUT  ***/
        xerror = xnor_model_evaluate(xmodel, xinput, NULL, &xresult);

        if(xerror != NULL){
            qDebug() << "xnor_model_evaluate error!\n"
                     << xnor_error_get_description(xerror);
        }
        qDebug() << "xnor_model_evaluate successful!";

        /*** GET THE TYPE OF AN EVALUATION RESULT ***/
        xtype = xnor_evaluation_result_get_type(xresult);

        switch (xtype) {
        case 0:
            qDebug() << "Unknown result type";
            break;
        case 1:
            qDebug() << "Bounding Boxes result type";
            break;
        case 2:
            qDebug() << "Class Labels result type";
            break;
        }


        FilterResult *r = new FilterResult;

        // clean last patterns detected
        r->m_bboxes.clear();
        r->m_class_ids.clear();
        r->m_confidence.clear();
        r->m_labels.clear();

        out_size = xnor_evaluation_result_get_bounding_boxes(xresult, bbox, MAX_OUT_SIZE);

        qDebug() << "FOUND " << out_size << "BOUNDING BOXES";

        xnor_evaluation_result_free(xresult);

        for(int i=0; i<out_size; i++){
            r->m_bboxes.append(QRect(bbox[i].rectangle.x*input->width(), bbox[i].rectangle.y*input->height(),
                                     bbox[i].rectangle.width*input->width(), bbox[i].rectangle.height*input->height()));
            r->m_class_ids.append(bbox[i].class_label.class_id);
            r->m_confidence.append(bbox[i].class_label.confidence*100);
            r->m_labels.append(bbox[i].class_label.label);
        }


        emit m_frameGrabber->finished(r);
    }

    input->unmap();

    // free imgBytes
    delete imgBytes;
    imgBytes = NULL;

    xnor_model_free(xmodel);
    xnor_error_free(xerror);
    xnor_input_free(xinput);

    return QVideoFrame();
}

