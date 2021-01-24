//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  Alex Nankervis
//

#include "Model.h"
#include <string.h>
#include <float.h>

Model::Model()
    : m_pMesh(nullptr)
    , m_pMaterial(nullptr)
    , m_pVertexData(nullptr)
    , m_pIndexData(nullptr)
    , m_pVertexDataDepth(nullptr)
    , m_pIndexDataDepth(nullptr)
    , m_SRVs(nullptr)
{
    Clear();
}

Model::~Model()
{
    Clear();
}

void Model::Clear()
{
    m_VertexBuffer.Destroy();
    m_IndexBuffer.Destroy();
    m_VertexBufferDepth.Destroy();
    m_IndexBufferDepth.Destroy();

    delete [] m_pMesh;
    m_pMesh = nullptr;
    m_Header.meshCount = 0;

    delete [] m_pMaterial;
    m_pMaterial = nullptr;
    m_Header.materialCount = 0;

    delete [] m_pVertexData;
    delete [] m_pIndexData;
    delete [] m_pVertexDataDepth;
    delete [] m_pIndexDataDepth;

    m_pVertexData = nullptr;
    m_Header.vertexDataByteSize = 0;
    m_pIndexData = nullptr;
    m_Header.indexDataByteSize = 0;
    m_pVertexDataDepth = nullptr;
    m_Header.vertexDataByteSizeDepth = 0;
    m_pIndexDataDepth = nullptr;

    ReleaseTextures();

    m_Header.boundingBox.min = Vector3(0.0f);
    m_Header.boundingBox.max = Vector3(0.0f);
}

// assuming at least 3 floats for position
void Model::ComputeMeshBoundingBox(unsigned int meshIndex, BoundingBox &bbox) const
{
    const Mesh *mesh = m_pMesh + meshIndex;

    if (mesh->vertexCount > 0)
    {
        unsigned int vertexStride = mesh->vertexStride;

        const float *p = (float*)(m_pVertexData + mesh->vertexDataByteOffset + mesh->attrib[attrib_position].offset);
        const float *pEnd = (float*)(m_pVertexData + mesh->vertexDataByteOffset + mesh->vertexCount * mesh->vertexStride + mesh->attrib[attrib_position].offset);
        bbox.min = Scalar(FLT_MAX);
        bbox.max = Scalar(-FLT_MAX);

        while (p < pEnd)
        {
            Vector3 pos(*(p + 0), *(p + 1), *(p + 2));

            bbox.min = Min(bbox.min, pos);
            bbox.max = Max(bbox.max, pos);

            (*(uint8_t**)&p) += vertexStride;
        }
    }
    else
    {
        bbox.min = Scalar(0.0f);
        bbox.max = Scalar(0.0f);
    }
}

void Model::ComputeGlobalBoundingBox(BoundingBox &bbox) const
{
    if (m_Header.meshCount > 0)
    {
        bbox.min = Scalar(FLT_MAX);
        bbox.max = Scalar(-FLT_MAX);
        for (unsigned int meshIndex = 0; meshIndex < m_Header.meshCount; meshIndex++)
        {
            const Mesh *mesh = m_pMesh + meshIndex;

            bbox.min = Min(bbox.min, mesh->boundingBox.min);
            bbox.max = Max(bbox.max, mesh->boundingBox.max);
        }
    }
    else
    {
        bbox.min = Scalar(0.0f);
        bbox.max = Scalar(0.0f);
    }
}

void Model::ComputeAllBoundingBoxes()
{
    for (unsigned int meshIndex = 0; meshIndex < m_Header.meshCount; meshIndex++)
    {
        Mesh *mesh = m_pMesh + meshIndex;
        ComputeMeshBoundingBox(meshIndex, mesh->boundingBox);
    }
    ComputeGlobalBoundingBox(m_Header.boundingBox);
}

void make_plane(int width, int height, unsigned char* vertices, uint16_t* indices, int vertex_stride)
{
	width++;
	height++;

	//int size = sizeof(CIwFVec2);
	// Set up vertices
	for (int y = 0; y < height; y++)
	{
		int base = y * width;
		for (int x = 0; x < width; x++)
		{
			int index = base + x;
			unsigned char* v = vertices + index * vertex_stride;
			float* pos_uv = (float*)v;

			pos_uv[0] = (float)x * 100.0f;
			pos_uv[1] = 0.0f;
			pos_uv[2] = (float)y * 100.0f;

			pos_uv[3] = ((float)x) / width;
			pos_uv[4] = ((float)y) / height;
		}
	}

	// Set up indices
	int i = 0;
	height--;
	for (int y = 0; y < height; y++)
	{
		int base = y * width;

		//indices[i++] = (uint16)base;
		for (int x = 0; x < width; x++)
		{
			indices[i++] = (uint16_t)(base + x);
			indices[i++] = (uint16_t)(base + width + x);
		}
		// add a degenerate triangle (except in a last row)
		if (y < height - 1)
		{
			indices[i++] = (uint16_t)((y + 1) * width + (width - 1));
			indices[i++] = (uint16_t)((y + 1) * width);
		}
	}
}

void Model::GenerateWater()
{
	m_pMesh = new Mesh();
	m_pMaterial = new Material;

	m_Header.meshCount = 1;
	m_Header.materialCount = 1;	

	m_VertexStride = 3 * sizeof(float) + 2 * sizeof(float);//m_pMesh[0].vertexStride;

	int row = 10;
	int colume = 10;
	int total_vertices = (row + 1) * (colume + 1);

	int numIndPerRow = colume * 2 + 2;
	int numIndDegensReq = (row - 1) * 2;
	int total_indices = numIndPerRow * row + numIndDegensReq;

	m_pMesh->materialIndex = 0;
	m_pMesh->vertexCount = total_vertices;
	m_pMesh->vertexDataByteOffset = 0;
	m_pMesh->indexCount = total_indices;
	m_pMesh->indexDataByteOffset = 0;

	//planeInd = new uint16[total_indices];	

	m_Header.vertexDataByteSize = m_VertexStride * total_vertices;
	m_Header.indexDataByteSize = sizeof(uint16_t) * total_indices;

	m_pVertexData = new unsigned char[m_Header.vertexDataByteSize];
	m_pIndexData = new unsigned char[m_Header.indexDataByteSize];
	//m_pVertexDataDepth = new unsigned char[m_Header.vertexDataByteSizeDepth];
	//m_pIndexDataDepth = new unsigned char[m_Header.indexDataByteSize];

	/*if (m_Header.vertexDataByteSize > 0)
		if (1 != fread(m_pVertexData, m_Header.vertexDataByteSize, 1, file)) goto h3d_load_fail;
	if (m_Header.indexDataByteSize > 0)
		if (1 != fread(m_pIndexData, m_Header.indexDataByteSize, 1, file)) goto h3d_load_fail;

	if (m_Header.vertexDataByteSizeDepth > 0)
		if (1 != fread(m_pVertexDataDepth, m_Header.vertexDataByteSizeDepth, 1, file)) goto h3d_load_fail;
	if (m_Header.indexDataByteSize > 0)
		if (1 != fread(m_pIndexDataDepth, m_Header.indexDataByteSize, 1, file)) goto h3d_load_fail;*/

	make_plane(colume, row, m_pVertexData, (uint16_t*)m_pIndexData, m_VertexStride);

	m_VertexBuffer.Create(L"VertexBuffer", m_Header.vertexDataByteSize / m_VertexStride, m_VertexStride, m_pVertexData);
	m_IndexBuffer.Create(L"IndexBuffer", m_Header.indexDataByteSize / sizeof(uint16_t), sizeof(uint16_t), m_pIndexData);
	delete[] m_pVertexData;
	m_pVertexData = nullptr;
	delete[] m_pIndexData;
	m_pIndexData = nullptr;

	/*m_VertexBufferDepth.Create(L"VertexBufferDepth", m_Header.vertexDataByteSizeDepth / m_VertexStrideDepth, m_VertexStrideDepth, m_pVertexDataDepth);
	m_IndexBufferDepth.Create(L"IndexBufferDepth", m_Header.indexDataByteSize / sizeof(uint16_t), sizeof(uint16_t), m_pIndexDataDepth);
	delete[] m_pVertexDataDepth;
	m_pVertexDataDepth = nullptr;
	delete[] m_pIndexDataDepth;
	m_pIndexDataDepth = nullptr;*/

	LoadTextures();
}