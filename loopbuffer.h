#pragma once
#ifndef DSY_LOOP_H
#define DSY_LOOP_H
#include <stdlib.h>
#include <stdint.h>
namespace daisysp
{
/** Delay line buffer for looper applications.
This is a modification of delayline.h in DaisySP library.
March 2021

declaration example: (1 second of floats)

LoopBuffer<float, SAMPLE_RATE> del;

By: Shahin Etemadzadeh
*/
template <typename T, size_t max_size>
class LoopBuffer
{
  public:
    LoopBuffer() {}
    ~LoopBuffer() {}
    /** initializes the buffer by clearing the values within, and setting length to 1 sample.
    */
    void Init() { 
        for(size_t i = 0; i < max_size; i++)
        {
            line_[i] = T(0);
        }
        Reset(); 
    }
    /** sets write ptr and read ptr to 0, and length to 1 sample.
    */
    void Reset()
    {
        write_ptr_  = 0;
        read_ptr_   = 0;
        length_     = 1;
        frac_       = 0.f;
    }

    /** sets the buffer length time in samples
        If a float is passed in, a fractional component will be calculated for interpolating the delay line.
    */
    inline void SetLength(size_t length)
    {
        frac_  = 0.0f;
        //length_ = length < max_size ? length : max_size - 1;
        length_ = length < max_size ? length : max_size;        //SE 2021116: Trying to fix overrun issues with Continuous Looper
    }

    /** sets the buffer length time in samples
        If a float is passed in, a fractional component will be calculated for interpolating the delay line.
    */
    inline void Setlength(float length)
    {
        int32_t int_length = static_cast<int32_t>(length);
        frac_             = length - static_cast<float>(int_length);
        length_ = static_cast<size_t>(int_length) < max_size ? int_length
                                                           : max_size - 1;
    }

    /** returns the buffer length in samples as a float
    */
    inline float GetLength()
    {
        float l = static_cast<float>(length_) + frac_;
        return l;
    }

    /** sets the read pointer position in samples
     *  If a float is passed, a fractional component will be ignored
     *  If the position is outside the bounds of the loop, the position is bounded between 0 and the length of the loop
    */
    inline void SetReadPosition(size_t position)
    {
        read_ptr_ = position >= length_ ? position : length_ - 1;  //read_ptr_ must be bounded on the loop
        read_ptr_ = read_ptr_ < 0 ? 0 : read_ptr_;                         //read_ptr_ must be bounded on the loop
    }

    /** returns the position of the read pointer
    */
    inline size_t GetReadPosition()
    {
        size_t rp = read_ptr_;
        return rp;
    }

    /** returns the position of the write pointer
    */
    inline size_t GetWritePosition()
    {
        size_t wp = write_ptr_;
        return wp;
    }

    /** writes the sample of type T to the delay line, and advances the write ptr while dynamically updating the length
    */
    inline void Write(const T sample)
    {
        line_[write_ptr_] = sample;
        write_ptr_        = (write_ptr_ + 1) % max_size;
        if (write_ptr_ >= length_) { 
            length_ = write_ptr_ + 1;
        }
    }

    /** returns the next sample of type T in the buffer, interpolated if necessary, and increments position of read pointer.
    */
    inline const T Read() //const
    {
        T a = line_[(read_ptr_) % length_];
        //T b = line_[(read_ptr_ + 1) % length_];
        read_ptr_ = (read_ptr_ + 1) % length_;
        //return a + (b - a) * frac_;
        return a;
    }

    /** returns the next sample of type T in the buffer, interpolated if necessary, and increments position of read pointer .
     *  but does not loop back once complete
    */
    inline const T ReadOnce() //const
    {
        T a = 0;

        if (read_ptr_ < length_) {
            a = line_[(read_ptr_) % length_];
        }
        read_ptr_ = read_ptr_ < (length_ - 1) ? read_ptr_ + 1 : read_ptr_;

        return a;
    }

    /** returns the next sample of type T in the buffer, with a defined clip length
    */
    inline const T Read(float clipEnd, size_t minClip) //const
    {
        size_t newEnd = (size_t) (clipEnd * length_);
        newEnd = newEnd < minClip ? minClip : newEnd;

        //limit the length of the clip as specified by clipEnd
        T a = line_[(read_ptr_) % newEnd];
        read_ptr_ = (read_ptr_ + 1) % newEnd;
        
        return a;
    }

    /** returns the next sample of type T in the buffer, with a defined start point in the clip and a defined length
    */
    inline const T Read(float clipStart, float clipEnd, size_t minClip) //const
    {
        size_t newEnd = (size_t) (clipEnd * length_);
        newEnd = newEnd < minClip ? minClip : newEnd;
        size_t offset = (size_t) (clipStart * length_);

        //read the clip from the point specified by clipStart
        T a = line_[(read_ptr_ + offset) % length_];
        //limit the length of the clip as specified by clipEnd
        read_ptr_ = (read_ptr_ + 1) % newEnd;
        
        return a;
    }

     /** returns the next sample of type T in the buffer, with a defined start point in the clip and a defined length as well as
      * the ability to randomize the start point and length
      * float clipStart -   [0..1.0] Specifies where in the loop the clip will start.  If randomStart is true, this knob affects how
      *                     the clip startpoint deviates from the loop start point
      * float clipEnd   -   [0..1.0] Specifies how long the clip is.  If randomLength is true, sets the range of possible lengths.
      * size_T minClip  -   Size in samples of the shortest allowable clip.
    */
    inline const T Read(float clipStart, float clipEnd, size_t minClip, bool randomLength, bool randomStart) //const
    {
        static size_t newEnd;
        static size_t offset;
        T a;

        if (!randomStart) {
            //select where in the clip is our starting point based on the knob position
            offset = (size_t) (clipStart * length_);
        } else {
            if (GetReadPosition() == 0) {
                //At the start of each clip loop
                //generate a random number between 0 and length_-1 and then mult by clipStart
                //the higher clipStart is, the more random the starting point is from 0
                offset = (size_t) (clipStart * (rand() % length_));
            }
        }

        if (!randomLength) {
            newEnd = (size_t) (clipEnd * length_);
        } else {
            //At the start of each clip loop
            //generate a random number between 0 and length_-1 and then mult by clipEnd
            //the higher clipEnd is, the more random the clip length will stray from half of the clip length
            if (GetReadPosition() == 0) {
                newEnd = (size_t) (clipEnd * (rand() % length_));
            }
        }
        //boundary check
        newEnd = newEnd < minClip ? minClip : newEnd;
        newEnd = newEnd >= length_ ? length_ : newEnd;

        //read the clip from the point specified by clipStart
        a = line_[(read_ptr_ + offset) % length_];
        //limit the length of the clip as specified by clipEnd
        read_ptr_ = (read_ptr_ + 1) % newEnd;
        
        
        return a;
    }

     /** returns the next sample of type T in the buffer, with a defined start point in the clip and a defined length as well as
      * the ability to randomize the start point and length
      * float clipStart -   [0..1.0] Specifies where in the loop the clip will start.  If randomStart is true, this knob affects how
      *                     the clip startpoint deviates from the loop start point
      * float clipEnd   -   [0..1.0] Specifies how long the clip is.  If randomLength is true, sets the range of possible lengths.
      * float speed     -   [-2.0..2.0] The speed at which the clips plays back
      * size_T minClip  -   Size in samples of the shortest allowable clip.
    */
    inline const T Read(float clipStart, float clipEnd, float speed, size_t minClip, bool randomLength, bool randomStart) //const
    {
        static size_t newEnd;
        static size_t offset;
        size_t rpo;
        T a, b;
        float intpart;
        if (length_ > 1) {
            if (!randomStart) {
                //select where in the clip is our starting point based on the knob position
                offset = (size_t) (clipStart * length_);
            } else {
                if (GetReadPosition() == 0) {
                    //At the start of each clip loop
                    //generate a random number between 0 and length_-1 and then mult by clipStart
                    //the higher clipStart is, the more random the starting point is from 0
                    offset = (size_t) (clipStart * (rand() % length_));
                    if (offset < 10) { offset = 10; }   //hack to get rid of clicks
                }
            }

            if (!randomLength) {
                newEnd = (size_t) (clipEnd * length_);
            } else {
                //At the start of each clip loop
                //generate a random number between 0 and length_-1 and then mult by clipEnd
                //the higher clipEnd is, the more random the clip length will stray from half of the clip length
                if (GetReadPosition() == 0) {
                    newEnd = (size_t) (clipEnd * (rand() % length_));
                }
            }
            //boundary check
            newEnd = newEnd < minClip ? minClip : newEnd;
            newEnd = newEnd >= length_ ? length_ : newEnd;

            //read the clip from the point specified by clipStart
            rpo = read_ptr_ + offset;
            a = line_[(rpo) % length_];
            if (speed >= 0.f) {
                b = line_[(rpo + 1) % length_];
            } else {
                b = (rpo - 1) > 0 ? line_[(rpo - 1) % length_] : length_ - 1;
            }

            frac_ = modf((speed + frac_), &intpart);
            read_ptr_ = (read_ptr_ + (int32_t)(intpart)) % newEnd;
            if (read_ptr_ < 0) { read_ptr_ = newEnd - 1; }
            
            return a + (b - a) * frac_;
        } else {
            return 0.f;
        }
    }


    /** returns the next sample of type T in the buffer, interpolated if necessary, and increments position of read pointer
        at a variable read speed.
    */
    inline const T ReadSpeed(float speed) //const
    {
        read_ptr_ = (read_ptr_ + (int)floor(speed + frac_)) % length_;
        if (read_ptr_ < 0) { read_ptr_ = length_ - 1; }     //SE20211103: this accounts for negative speeds
        frac_ = speed + frac_ - static_cast<int32_t>(speed + frac_);
        T a = line_[(read_ptr_) % length_];
        T b = line_[(read_ptr_ + 1) % length_];
        /*if (speed >= 0) {
            b = line_[(read_ptr_ + 1) % length_];
        } else {
            b = line_[(read_ptr_ + 1) % length_];
        }*/
        
        return a + (b - a) * frac_;
    }

    /** forces a smooth transition between the start and end of a loop
    */
    inline void Splice()
    {
        //TODO: Check if loop is longer than fadeLength
        int fadeLength = 2048;
        /*for (int i = 0; i < fadeLength; i++) {
            line_[((int)length_ - fadeLength + i)] = ((i/fadeLength)*(i/fadeLength) * line_[fadeLength - (i+1)]) + ((1 - (i/fadeLength)*(i/fadeLength)) * line_[((int)length_ - fadeLength + i)]);
        }*/
        //first order lowpass to smooth transition
        //T prev = line_[length_ - 1 - fadeLength];
        //T coef = 0.0000571;                       //25Hz
        //for (int j = 0; j < 8; j++) {

            for (int i = 0; i < fadeLength; i++) {
                line_[i]                 *= (float)i / (float)fadeLength;
                line_[(length_ - 1) - i] *= (float)i / (float)fadeLength;
            }
        //}

    }

    inline void Splice(int fadeLength, int startPoint, int endPoint)
    {
        if ((2*fadeLength < (int)length_) && (startPoint >= 0) && (endPoint < (int)length_))
        {
            for (int i = 0; i < fadeLength; i++) {
                line_[startPoint + i] *= (float)i / (float)fadeLength;
                line_[endPoint - i]   *= (float)i / (float)fadeLength;
            }
            //zero out remaing portion of the buffer
            for (int i = endPoint; i < (int)length_; i++) {
                line_[i] = 0.f;
            }    
        }
        
    }

  private:
    float  frac_;
    size_t write_ptr_;
    size_t read_ptr_;
    size_t length_;
    T      line_[max_size];
};
} // namespace daisysp
#endif
