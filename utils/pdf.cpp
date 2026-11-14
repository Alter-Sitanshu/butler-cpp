#include "../include/pdf.hpp"
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <podofo/main/PdfEncrypt.h>
#include <podofo/main/PdfError.h>
#include <podofo/main/PdfMemDocument.h>
#include <stdexcept>
#include <string>

#include <podofo/podofo.h>

void mergePDF(const Args& args) {

    bool verbose = args.getBool("verbose");
    
    if (!verbose) {
        PoDoFo::PdfCommon::SetMaxLoggingSeverity(PoDoFo::PdfLogSeverity::Error);
    }

    std::vector<std::string> files = args.getPosArgs();

    assert (files.size() > 1 && "Minimum of 2 files are needed to merge. Sorry !\n");

    std::string output = args.getStr("output");
    
    assert (output.size() > 4 && "Filename invalid for output PDF\n");
    std::string extension = output.substr(output.size() - 4, 4);
    
    assert (extension == ".pdf" && "Output file extension mismatch. expected .pdf output file\n");
    for (std::string& file : files) {
        assert((file.size() > 4 && file.substr(file.size() - 4, 4) == ".pdf") || "File extension mismatch. expected a .pdf file\n");
    }

    // the first files is always loaded first so array should be 1 size less
    std::vector<PoDoFo::PdfMemDocument> docs;
    docs.reserve(files.size() - 1);

    PoDoFo::PdfMemDocument out_pdf;
    out_pdf.Load(files[0]);


    for (size_t i = 1; i < files.size(); ++i) {
        try {
            docs[i-1].Load(files[i]);
            int p = docs[i-1].GetPages().GetCount();
            if (p > 35) {
                throw std::invalid_argument("file too large to merge. sorry!\n");
            }
            out_pdf.GetPages().AppendDocumentPages(docs[i-1]);

        } catch (PoDoFo::PdfError& e) {
            std::fprintf(
                    stderr, "Error occured while opening the PDF file (%s): %s\n",
                    files[i].c_str(), e.what()
            );
        }

    }

    out_pdf.Save(output);

};


void protectPDF(const Args& args) {
    
    std::string owner_pwd;
    std::string user_pwd;
    
    std::vector<std::string> targets = args.getPosArgs();
    size_t N = targets.size();

    for (size_t i = 0; i < N; ++i) {
        size_t l = targets[i].size();
        assert (l > 4 || "filename invalid. must be greater than 4 characters");
        std::string ext{targets[i].substr(l-4, 4)}; 
        assert (ext  == ".pdf" || "invalid file type provided.");
        if (ext != ".pdf") {
            std::fprintf(stderr, "got(%s), wanted(.pdf)", ext.c_str());
            return;
        }
    }


    bool setAll = args.getBool("all");
    std::string perm_arg = args.getStr("perm");
    
    PoDoFo::PdfPermissions perm;
    if (perm_arg == "default") {
        perm = PoDoFo::PdfPermissions::Default;
    } else if (perm_arg == "copy") {
        perm = PoDoFo::PdfPermissions::Copy;
    } else if (perm_arg == "access") {
        perm = PoDoFo::PdfPermissions::Accessible;
    } else if (perm_arg == "edit") {
        perm = PoDoFo::PdfPermissions::Edit;
    } else if (perm_arg == "print") {
        perm = PoDoFo::PdfPermissions::Print;
    } else {
        throw std::invalid_argument("invlaid flag value provided for permissions - allowed: print, default, edit, copy, access");
    }

    std::vector<PoDoFo::PdfMemDocument> docs(N);
    // docs.reserve(N);
    std::string output_path{};

    if (setAll) {
        std::printf("Enter the owner password: ");
        std::getline(std::cin, owner_pwd);

        std::printf("Enter user password: ");
        std::getline(std::cin, user_pwd);


        strip_str(owner_pwd);
        strip_str(user_pwd);
        // Set the encryption with default permissions for the document
        for (size_t i = 0; i < N; ++i) {
            try {
                std::printf("Reached Checkpoint 1\n");
                output_path.assign("protected_" + targets[i]);
                docs[i].Load(targets[i]);
                docs[i].SetEncrypted(user_pwd, owner_pwd, perm);
                docs[i].Save(output_path);
            } catch (PoDoFo::PdfError& e) {
                std::printf("error while opening the file: %s, what: %s\n", targets[i].c_str(), e.what());
                continue;
            }
        }

        return;
    }

    for (size_t i = 0; i < N; ++i) {
        std::printf("Accepting password for %s\n", targets[i].c_str());
        std::printf("Enter the owner password: ");
        std::getline(std::cin, owner_pwd);

        std::printf("Enter user password: ");
        std::getline(std::cin, user_pwd);


        strip_str(owner_pwd);
        strip_str(user_pwd);
        
        try {
            output_path.assign("protected_" + targets[i]);
            docs[i].Load(targets[i]);
            docs[i].SetEncrypted(user_pwd, owner_pwd, perm);
            docs[i].Save(output_path);
        } catch (PoDoFo::PdfError& e) {
            std::printf("error while opening the file: %s, what: %s\n", targets[i].c_str(), e.what());
            continue;
        }
    }
};
