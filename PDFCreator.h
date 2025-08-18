#include <iostream>
#include <string>
#include <hpdf.h>
#include <stdexcept>
#include "Logger.h"

class PDFCreator {
private:
    HPDF_Doc m_pdf;
    HPDF_Page m_currentPage;
    HPDF_Font m_font;
    float m_currentY;
    float m_fontSize;
    float m_topMargin;
    float m_bottomMargin;
    float m_lineSpacing;
    bool m_textBlockActive;
    int m_pageCount;    // Track total pages
    int m_lineCount;    // Track total lines of text
    size_t lastSpacePos = 0;

    void createNewPage() {
        if (m_textBlockActive) {
            HPDF_Page_EndText(m_currentPage);
            m_textBlockActive = false;
        }
        
        m_currentPage = HPDF_AddPage(m_pdf);
        // Add critical NULL check for page creation
        if (!m_currentPage) {
            LOG_ERROR("Failed to create new page");
            throw std::runtime_error("Failed to create new page");
        }
        
        HPDF_Page_SetSize(m_currentPage, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
        m_currentY = HPDF_Page_GetHeight(m_currentPage) - m_topMargin;
        
        HPDF_Page_SetFontAndSize(m_currentPage, m_font, m_fontSize);
        HPDF_Page_BeginText(m_currentPage);
        m_textBlockActive = true;
        m_pageCount++; // Increment page count
   
    }

    void ensureSpaceForText() {
        if (m_currentY - m_lineSpacing < m_bottomMargin) {
            createNewPage();
        }
    }

    void ensureSpaceForImage(float imageHeight) {
        float requiredSpace = imageHeight + 10; // Add small margin
        while (m_currentY - requiredSpace < m_bottomMargin) {
            createNewPage();
        }
    }

public:
    PDFCreator() 
        : m_pdf(HPDF_New(error_handler, nullptr)),
          m_currentPage(nullptr),
          m_font(nullptr),
          m_currentY(0),
          m_fontSize(15),
          m_topMargin(50),
          m_bottomMargin(50),
          m_lineSpacing(20 * 1.2),
          m_textBlockActive(false),
          m_pageCount(0),
          m_lineCount(0) {
            LOG_INFO("PDFCreator Constructor");        
            reset();
    }

    ~PDFCreator() {
        LOG_INFO("PDFCreator Destructor");
        if (m_pdf) HPDF_Free(m_pdf);
    }

    void addText(const std::string& text) {
        if (text.empty()) return;
        
        // Ensure we have an active text block
        if (!m_textBlockActive) {
            HPDF_Page_BeginText(m_currentPage);
            m_textBlockActive = true;
        }
        
        // Get current page dimensions
        const float pageWidth = HPDF_Page_GetWidth(m_currentPage) - 100; // 50px margins on both sides
        const float maxLineWidth = pageWidth - 50; // Additional safety margin

        std::string remainingText = text;
        
        while (!remainingText.empty()) {
            ensureSpaceForText(); // Check if we need a new page
            
            // Measure how much text fits in one line
            HPDF_REAL width = 0;
            size_t charsThatFit = 0;
            bool foundSpace = false;
            
            // Find the maximum number of characters that fit
            for (size_t i = 1; i <= remainingText.length(); ++i) {
                std::string testStr = remainingText.substr(0, i);
                width = HPDF_Page_TextWidth(m_currentPage, testStr.c_str());
                
                if (width > maxLineWidth) {
                    // If we exceeded width, back up to last space if possible
                    if (foundSpace) {
                        charsThatFit = lastSpacePos;
                    } else {
                        // If no space found, just break at current position
                        charsThatFit = i - 1;
                    }
                    break;
                }
                
                // Remember last space position for word wrapping
                if (remainingText[i-1] == ' ') {
                    lastSpacePos = i;
                    foundSpace = true;
                }
                
                // If we reached end of string
                if (i == remainingText.length()) {
                    charsThatFit = i;
                }
            }

            // Extract the portion that fits
            std::string line = remainingText.substr(0, charsThatFit);
            remainingText = remainingText.substr(charsThatFit);
            
            // Trim leading whitespace from remaining text
            while (!remainingText.empty() && remainingText[0] == ' ') {
                remainingText.erase(0, 1);
            }

            // Output the line
            HPDF_Page_TextOut(m_currentPage, 50, m_currentY, line.c_str());
            m_currentY -= m_lineSpacing;
            m_lineCount++;
        }
    }

    void reset() {
        // Clean up existing PDF document
        if (m_pdf) {
            HPDF_Free(m_pdf);
            m_pdf = nullptr;
        }
        
        // Reset all internal state
        m_currentPage = nullptr;
        m_font = nullptr;
        m_currentY = 0;
        m_pageCount = 0;
        m_lineCount = 0;
        m_textBlockActive = false;
        
        // Create a new PDF document
        m_pdf = HPDF_New(error_handler, nullptr);
        if (!m_pdf) {
            LOG_ERROR("Failed to create PDF document");
            throw std::runtime_error("Failed to create PDF document");
        }

        // Enable UTF-8 support
        HPDF_UseUTFEncodings(m_pdf);
        HPDF_SetCurrentEncoder(m_pdf, "UTF-8");

        // Load the TrueType font
        const char* font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
        const char* font_name = HPDF_LoadTTFontFromFile(m_pdf, font_path, HPDF_TRUE);

        // Get the font AFTER loading it
        m_font = HPDF_GetFont(m_pdf, font_name, "UTF-8"); // Use "DejaVu Sans" with space
        if (!m_font) {
            LOG_ERROR("Failed to get DejaVu Sans font with UTF-8 encoding");
            throw std::runtime_error("Font not found");
        }

        createNewPage();
    }

    void addImage(const std::string& imagePath) {
        if (m_textBlockActive) {
            HPDF_Page_EndText(m_currentPage);
            m_textBlockActive = false;
        }

        // Load image using Qt to resize/compress
        QImage img(QString::fromStdString(imagePath));
        if (img.isNull()) {
            LOG_ERROR("Failed to load image: " + imagePath);
            throw std::runtime_error("Invalid image: " + imagePath);
        }

        // Resize to max 800px width (adjust as needed)
        img = img.scaledToWidth(640, Qt::SmoothTransformation);

        // Save as JPEG (quality 80) to a temp file
        QString tempPath = QString::fromStdString(imagePath) + "_compressed.jpg";
        if (!img.save(tempPath, "JPEG", 80)) {
            LOG_ERROR("Failed to compress image");
            throw std::runtime_error("Image compression failed");
        }

        // Load the compressed image into PDF
        HPDF_Image image = HPDF_LoadJpegImageFromFile(m_pdf, tempPath.toStdString().c_str());
        if (!image) {
            LOG_ERROR("Failed to load compressed image");
            throw std::runtime_error("PDF image load failed");
        }

        // Proceed with original scaling/placement logic
        float imageHeight = HPDF_Image_GetHeight(image);
        float imageWidth = HPDF_Image_GetWidth(image);
        float maxWidth = HPDF_Page_GetWidth(m_currentPage) - 100;
        float scale = (imageWidth > maxWidth) ? (maxWidth / imageWidth) : 1.0f;
        
        HPDF_Page_DrawImage(m_currentPage, image,
            50, m_currentY - (imageHeight * scale),
            imageWidth * scale, imageHeight * scale
        );
        
        m_currentY -= (imageHeight * scale + 10);
        QFile::remove(tempPath); // Clean up temp file
    }
    void addImage1(const std::string& imagePath) {
        if (m_textBlockActive) {
            HPDF_Page_EndText(m_currentPage);
            m_textBlockActive = false;
        }
    
        HPDF_Image image = HPDF_LoadPngImageFromFile(m_pdf, imagePath.c_str());
        if (!image) {
            LOG_ERROR("Failed to load image: " + imagePath);
            throw std::runtime_error("Failed to load image: " + imagePath);
        }
    
        float imageHeight = HPDF_Image_GetHeight(image);
        float imageWidth = HPDF_Image_GetWidth(image);
        
        if (imageWidth <= 0 || imageHeight <= 0) {
            LOG_ERROR("Invalid image dimensions for: " + imagePath);
            throw std::runtime_error("Invalid image dimensions for: " + imagePath);
        }
        
        float maxWidth = HPDF_Page_GetWidth(m_currentPage) - 100;
        float scale = (imageWidth > maxWidth) ? (maxWidth / imageWidth) : 1.0f;
        float scaledWidth = imageWidth * scale;
        float scaledHeight = imageHeight * scale;
    
        ensureSpaceForImage(scaledHeight);
    
        // Critical Fix: End text block if active after space checks
        if (m_textBlockActive) {
            HPDF_Page_EndText(m_currentPage);
            m_textBlockActive = false;
        }
    
        HPDF_Page_DrawImage(m_currentPage, image,
                          50, m_currentY - scaledHeight,
                          scaledWidth, scaledHeight);
        
        m_currentY -= (scaledHeight + 10);
    }

    void saveToFile(const std::string& filename) {
        if (m_textBlockActive) {
            HPDF_Page_EndText(m_currentPage);
            m_textBlockActive = false;
        }
        
        if (HPDF_SaveToFile(m_pdf, filename.c_str()) != HPDF_OK) {
            LOG_ERROR("Failed to save PDF file");
            throw std::runtime_error("Failed to save PDF file");
        }
    }

    static void error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void*) {
        std::cerr << "PDF Error: code=0x" << std::hex << error_no 
                 << ", detail=" << std::dec << detail_no << std::endl;
        throw std::runtime_error("PDF operation failed");
    }

    int getPageCount() const { return m_pageCount; }
    int getLineCount() const { return m_lineCount; }
    // Disable copy semantics
    PDFCreator(const PDFCreator&) = delete;
    PDFCreator& operator=(const PDFCreator&) = delete;
};