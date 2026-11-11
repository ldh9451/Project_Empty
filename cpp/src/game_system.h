#pragma once

namespace project_h
{

    class IGameSystem
    {
    public:
        virtual ~IGameSystem() = default;
        virtual void Init() = 0;
        virtual void Update(double delta) = 0;
        virtual void Release() = 0;
    };

} // namespace project_h
