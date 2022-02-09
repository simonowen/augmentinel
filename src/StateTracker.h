#pragma once

class D3D11StateTracker
{
public:
	D3D11StateTracker(ID3D11DeviceContext* pDeviceContext)
		: m_pDeviceContext(pDeviceContext) { }

	void SetVertexBuffer(ID3D11Buffer* pVertexBuffer, UINT stride, UINT offset = 0)
	{
		if (pVertexBuffer && pVertexBuffer != m_pLastVertexBuffer)
		{
			m_pDeviceContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
			m_pLastVertexBuffer = pVertexBuffer;
		}
	}

	void SetIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT format)
	{
		if (pIndexBuffer && pIndexBuffer != m_pLastIndexBuffer)
		{
			m_pDeviceContext->IASetIndexBuffer(pIndexBuffer, format, 0);
			m_pLastIndexBuffer = pIndexBuffer;
		}
	}

	void SetInputLayout(ID3D11InputLayout* pInputLayout)
	{
		if (pInputLayout && pInputLayout != m_pLastInputLayout)
		{
			m_pDeviceContext->IASetInputLayout(pInputLayout);
			m_pLastInputLayout = pInputLayout;
		}
	}

	void SetRasterizerState(ID3D11RasterizerState* pRasterizerState)
	{
		if (pRasterizerState && pRasterizerState != m_pLastRasterizerState)
		{
			m_pDeviceContext->RSSetState(pRasterizerState);
			m_pLastRasterizerState = pRasterizerState;
		}
	}

	void SetSamplerState(ID3D11SamplerState* pSamplerState)
	{
		if (pSamplerState && pSamplerState != m_pLastSamplerState)
		{
			m_pDeviceContext->PSSetSamplers(0, 1, &pSamplerState);
			m_pLastSamplerState = pSamplerState;
		}
	}

	void SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY primitiveTopology)
	{
		if (primitiveTopology != m_LastPrimitiveTopology)
		{
			m_pDeviceContext->IASetPrimitiveTopology(primitiveTopology);
			m_LastPrimitiveTopology = primitiveTopology;
		}
	}

	void SetVertexShader(ID3D11VertexShader* pVertexShader)
	{
		if (pVertexShader && pVertexShader != m_pLastVertexShader)
		{
			m_pDeviceContext->VSSetShader(pVertexShader, nullptr, 0);
			m_pLastVertexShader = pVertexShader;
		}
	}

	void SetPixelShader(ID3D11PixelShader* pPixelShader)
	{
		if (pPixelShader && pPixelShader != m_pLastPixelShader)
		{
			m_pDeviceContext->PSSetShader(pPixelShader, nullptr, 0);
			m_pLastPixelShader = pPixelShader;
		}
	}

	void SetVertexConstants(ID3D11Buffer* pVertexConstants)
	{
		if (pVertexConstants && pVertexConstants != m_pLastVertexConstants)
		{
			m_pDeviceContext->VSSetConstantBuffers(0, 1, &pVertexConstants);
			m_pLastVertexConstants = pVertexConstants;
		}
	}

	void SetPixelConstants(ID3D11Buffer* pPixelConstants)
	{
		if (pPixelConstants && pPixelConstants != m_pLastPixelConstants)
		{
			m_pDeviceContext->PSSetConstantBuffers(0, 1, &pPixelConstants);
			m_pLastPixelConstants = pPixelConstants;
		}
	}

	void SetPixelResource(ID3D11ShaderResourceView* pShaderResourceView)
	{
		if (pShaderResourceView != m_pLastPixelShaderResourceView)
		{
			m_pDeviceContext->PSSetShaderResources(0, 1, &pShaderResourceView);
			m_pLastPixelShaderResourceView = pShaderResourceView;
		}
	}

protected:
	ComPtr<ID3D11DeviceContext> m_pDeviceContext;

	ID3D11Buffer* m_pLastVertexBuffer{ nullptr };
	ID3D11Buffer* m_pLastIndexBuffer{ nullptr };
	ID3D11InputLayout* m_pLastInputLayout{ nullptr };
	ID3D11RasterizerState* m_pLastRasterizerState{ nullptr };
	ID3D11SamplerState* m_pLastSamplerState{ nullptr };
	ID3D11VertexShader* m_pLastVertexShader{ nullptr };
	ID3D11PixelShader* m_pLastPixelShader{ nullptr };
	ID3D11Buffer* m_pLastVertexConstants{ nullptr };
	ID3D11Buffer* m_pLastPixelConstants{ nullptr };
	ID3D11ShaderResourceView* m_pLastPixelShaderResourceView{ nullptr };
	D3D11_PRIMITIVE_TOPOLOGY m_LastPrimitiveTopology{ D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED };
};
